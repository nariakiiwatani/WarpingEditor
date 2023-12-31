#include "ofApp.h"
#include "MeshData.h"
#include "GuiFunc.h"
#include "Icon.h"
#include "ImGuiFileDialog.h"

namespace {
template<typename T>
auto getJsonValue(const ofJson &json, const std::string &key, T default_value={}) -> T {
	return json.contains(key) ? json[key].get<T>() : default_value;
}
std::shared_ptr<ImageSource> buildTextureSource(const ProjectFolder &proj) {
	std::shared_ptr<ImageSource> ret = std::make_shared<ImageSource>();
	switch(proj.getTextureType()) {
		case ProjectFolder::Texture::FILE:
			if(ret->loadFromFile(proj.getTextureFilePath())) {
				return ret;
			}
			break;
		case ProjectFolder::Texture::NDI:
			if(ret->setupNDI(proj.getTextureNDIName())) {
				return ret;
			}
			break;
	}
	return nullptr;
}
}

std::string GuiApp::stateName(int state) const
{
	return std::vector<std::string>{"uv","warp","blend"}[state];
}

//--------------------------------------------------------------
void GuiApp::setup(){
	ofDisableArbTex();
	Icon::init();
	
//	ofEnableArbTex();
	ofBackground(ofColor::gray);

	gui_.setup(nullptr, true, ImGuiConfigFlags_DockingEnable, true);
	
	ndi_finder_.watchSources();
	
	warping_data_ = std::make_shared<WarpingData>();
	warp_uv_ = std::make_shared<WarpingUVEditor>();
	warp_uv_->setup();
	warp_uv_->setMeshData(warping_data_);
	warp_mesh_ = std::make_shared<WarpingMeshEditor>();
	warp_mesh_->setup();
	warp_mesh_->setMeshData(warping_data_);

	blending_data_ = std::make_shared<BlendingData>();
	blend_editor_ = std::make_shared<BlendingEditor>();
	blend_editor_->setup();
	blend_editor_->setMeshData(blending_data_);
	
	editor_ = {
		{"uv", warp_uv_},
		{"warp", warp_mesh_},
		{"blend", blend_editor_}
	};
	state_ = EDIT_BLEND;
	result_app_->setEditor(blend_editor_);

	// avoid weird flipping
	fbo_.allocate(1,1, GL_RGB);


	undo_.setup(this);

	loadRecent();
	openRecent();
	
	undo_.enableAuto(1);
}

void GuiApp::initUndo()
{
	undo_.clear();
	undo_.create();
	undo_.store();
}

//--------------------------------------------------------------
void GuiApp::update(){
	if(texture_source_) {
		auto tex = texture_source_->getTexture();
		texture_source_->update();
		if(texture_source_->isFrameNew()) {
			warp_uv_->setTexture(tex);
			warp_mesh_->setTexture(tex);
		}
		if(tex.isAllocated()) {
			glm::vec2 tex_size{tex.getWidth(), tex.getHeight()};
			glm::vec2 tex_size_cache = proj_.getTextureSizeCache();
			if(tex_size_cache != tex_size) {
				warping_data_->rescale(tex_size/tex_size_cache);
				proj_.setTextureSizeCache(tex_size);
			}
		}
	}
	bool gui_focused = ImGui::GetIO().WantCaptureMouse;
	for(auto &&e : editor_) {
		e.second->setEnableViewportEditByMouse(!gui_focused);
		e.second->setEnableMeshEditByMouse(!gui_focused);
	}

	auto editor = editor_[stateName(state_)];
	if(editor) {
		editor->setRegion(ofGetCurrentViewport());
		editor->update();
	}
	
	if(!warp_uv_->isPreventMeshInterpolation() && !warp_mesh_->isPreventMeshInterpolation()) {
		warping_data_->update();
	}
	if(texture_source_) {
		auto tex = texture_source_->getTexture();
		if(tex.isAllocated()) {
			auto tex_data = tex.getTextureData();
			glm::vec2 tex_scale = tex_data.textureTarget == GL_TEXTURE_RECTANGLE_ARB
			? glm::vec2{1,1}
			: glm::vec2{1/tex_data.tex_w, 1/tex_data.tex_h};
			auto warped_mesh = warping_data_->getMesh(100, tex_scale);
			fbo_.begin();
			ofClear(0);
			tex.bind();
			warped_mesh.draw();
			tex.unbind();
			fbo_.end();
		}
	}
	blend_editor_->setTexture(fbo_.getTexture());
}

//--------------------------------------------------------------
void GuiApp::draw(){
	auto editor = editor_[stateName(state_)];
	if(editor) {
		editor->draw();
	}
	
	gui_.begin();
	using namespace ImGui;
	Shortcut sc_save{[&]{save();}, 'S'};
	Shortcut sc_save_as{[&]{ImGuiFileDialog::Instance()->OpenModal("SaveProjDlgKey", "Save Project Folder", nullptr, ofToDataPath(""));}, 'S', Shortcut::MOD_FLAG_DEFAULT|ImGuiKeyModFlags_Shift};
	Shortcut sc_open{[&]{ImGuiFileDialog::Instance()->OpenModal("ChooseProjDlgKey", "Choose Project Folder", nullptr, ofToDataPath(""));}, 'O'};
	Shortcut sc_export{[&]{exportMesh(proj_);}, 'E'};
	bool is_open_export_dialog_trigger=false;
	Shortcut sc_export_to{[&]{is_open_export_dialog_trigger = true;}, 'E', Shortcut::MOD_FLAG_DEFAULT|ImGuiKeyModFlags_Shift};
	Shortcut sc_undo{[&]{undo();}, 'Z'};
	Shortcut sc_redo{[&]{redo();}, 'Z', Shortcut::MOD_FLAG_DEFAULT|ImGuiKeyModFlags_Shift};
	for(auto &&s : std::vector<Shortcut>{sc_save,sc_save_as,sc_open,sc_export,sc_export_to,sc_undo,sc_redo}) {
		if(s.check()) {
			s();
		}
	}

	std::vector<std::string> shortcut_triggers;
	if(BeginMainMenuBar()) {
		if(BeginMenu("Project")) {
			if(MenuItem("Open file...", sc_open.keyStr().c_str())) {
				sc_open();
			}
			Separator();
			if(BeginMenu("recent")) {
				for(auto &&r : recent_) {
					std::stringstream ss;
					ss << r.getRelative() << "(" << r.getAbsolute() << ")";
					if(MenuItem(ss.str().c_str())) {
						openProject(r.getAbsolute());
					}
				}
				ImGui::EndMenu();
			}
			Separator();
			if(MenuItem("Save", sc_save.keyStr().c_str())) {
				sc_save();
			}
			if(MenuItem("Save as...", sc_save_as.keyStr().c_str())) {
				sc_save_as();
			}
			ImGui::EndMenu();
		}
		if(BeginMenu("Edit")) {
			if(MenuItem("Undo", sc_undo.keyStr().c_str(), false, undo_.getUndoLength()>0)) {
				sc_undo();
			}
			if(MenuItem("Redo", sc_redo.keyStr().c_str(), false, undo_.getRedoLength()>0)) {
				sc_redo();
			}
			ImGui::EndMenu();
		}
		if(BeginMenu("Texture")) {
			if(BeginMenu("Image File")) {
				if(MenuItem("Load from file...")) {
					ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", "Choose File", "{.png,.gif,.jpg,.jpeg,.mov,.mp4}", ofFilePath::addTrailingSlash(proj_.getAbsolute().string()));
				}
				Separator();
				std::string filepath;
				if(SelectFileMenu(proj_.getRelative().string(), filepath, true, {"png","gif","jpg","jpeg","mov","mp4"})) {
					filepath = ofToDataPath(filepath, true);
					proj_.setTextureSourceFile(filepath);
					if((texture_source_ = buildTextureSource(proj_))) {
						auto tex = texture_source_->getTexture();
						if(tex.isAllocated()) {
							warp_uv_->setTexture(tex);
							warp_mesh_->setTexture(tex);
						}
					}
				}
				ImGui::EndMenu();
			}
			if(BeginMenu("NDI")) {
				auto source = ndi_finder_.getSources();
				for(auto &&s : source) {
					std::stringstream ss;
					ss << s.ndi_name << "(" << s.url_address << ")";
					if(MenuItem(ss.str().c_str())) {
						proj_.setTextureSourceNDI(s.ndi_name);
						texture_source_ = buildTextureSource(proj_);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if(BeginMenu("Export")) {
			if(MenuItem("export by recent settings", sc_export.keyStr().c_str())) {
				sc_export();
			}
			if(MenuItem("export...", sc_export_to.keyStr().c_str())) {
				sc_export_to();
			}
			ImGui::EndMenu();
		}
		EndMainMenuBar();
	}

	if(ImGuiFileDialog::Instance()->Display("SelectFolderDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			proj_.setExportFolder(filePathName);
			is_open_export_dialog_trigger = true;
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(is_open_export_dialog_trigger) {
		OpenPopup("is_open_export_dialog_trigger");
	}
	if(BeginPopupModal("is_open_export_dialog_trigger")) {
		std::string folder = proj_.getExportFolder();
		bool is_arb = proj_.getIsExportMeshArb();
		if(Checkbox("arb", &is_arb)) {
			proj_.setIsExportMeshArb(is_arb);
		}
		if(EditText("export folder", folder, 1024)) {
			proj_.setExportFolder(folder);
		} SameLine();
		if(Button("...")) {
			ImGuiFileDialog::Instance()->OpenModal("SelectFolderDlgKey", "Choose Export Folder", nullptr, folder);
		}
		if(TreeNodeEx("warping", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto param = proj_.getExportWarpParam();
			if(EditText("filename", param.filename)
			   || InputFloat("resample_min_interval", &param.max_mesh_size)) {
				proj_.setExportWarpParam(param);
			}
			TreePop();
		}
		if(TreeNodeEx("blending", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto param = proj_.getExportBlendParam();
			if(EditText("filename", param.filename)
			   || InputFloat("resample_min_interval", &param.max_mesh_size)) {
				proj_.setExportBlendParam(param);
			}
			auto sparam = proj_.getExportBlendShaderParam();
			if(EditText("shader_param", sparam.filename)) {
				proj_.setExportBlendShaderParam(sparam);
			}
			TreePop();
		}
		if(Button("export")) {
			sc_export();
			CloseCurrentPopup();
		} SameLine();
		if(Button("cancel")) {
			CloseCurrentPopup();
		}
		EndPopup();
	}
	if(ImGuiFileDialog::Instance()->Display("ChooseProjDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			openProject(filePathName);
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(ImGuiFileDialog::Instance()->Display("SaveProjDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			proj_.WorkFolder::setRelative(filePathName);
			save();
			proj_.setup();
			updateRecent(proj_);
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			proj_.setTextureSourceFile(filePathName);
			if((texture_source_ = buildTextureSource(proj_))) {
				auto tex = texture_source_->getTexture();
				if(tex.isAllocated()) {
					warp_uv_->setTexture(tex);
					warp_mesh_->setTexture(tex);
				}
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(Begin("status")) {
		if(TreeNode("undo/redo")) {
			int max_length = (int)undo_.getHistoryLengthLimit();
			if(InputInt("max length", &max_length)) {
				undo_.setHistoryLengthLimit(std::max(0, max_length));
			}
			if(IsItemHovered()) {
				SetTooltip("0: unlimited");
			}
			Text("current history length: %d", undo_.getUndoLength()+undo_.getRedoLength());
			Text("data size: %lukB", undo_.getDataSize()/1024);
			if(Button("clear")) {
				initUndo();
			}
			TreePop();
		}
	}
	End();
	if(Begin("ResultWindow")) {
		auto position = result_window_->getWindowPosition();
		if(DragFloat2("position", &position.x, 1, 0, -1, "%0.0f")) {
			result_window_->setWindowPosition(position.x, position.y);
		}
		auto size = result_window_->getWindowSize();
		if(DragFloat2("viewport_size", &size.x, 1, 1, -1, "%0.0f") && size.x > 0 && size.y > 0) {
			result_window_->setWindowShape(size.x, size.y);
		}
		auto fbo_size = glm::ivec2{fbo_.getWidth(), fbo_.getHeight()};
		if(InputInt2("texture_size", &fbo_size.x) && fbo_size.x > 0 && fbo_size.y > 0) {
			fbo_.allocate(fbo_size.x, fbo_size.y, GL_RGB);
			warp_mesh_->setBackgroundSize(fbo_size);
		}
		bool scale_to_viewport = result_app_->isScaleToViewport();
		if(Checkbox("scale_to_viewport", &scale_to_viewport)) {
			result_app_->setScaleToViewport(scale_to_viewport);
		}
		bool show_control = result_app_->isShowControl();
		if(Checkbox("show_control", &show_control)) {
			result_app_->setShowControl(show_control);
		}
		bool show_cursor = result_app_->isShowCursor();
		if(Checkbox("show_cursor", &show_cursor)) {
			result_app_->setShowCursor(show_cursor);
		}
		Text("%s", "Show Editor");
		for(auto &&e : editor_) {
			if(RadioButton(e.first.c_str(), e.second == result_app_->getEditor())) {
				result_app_->setEditor(e.second);
			}
		}
	}
	End();
	if(Begin("MeshList")) {
		if(isStateWarping()) {
			PushID("warping");
			warping_data_->gui(
							   [&](WarpingMesh &data) {
								   return state_ == EDIT_WARP_UV ? warp_uv_->isSelectedMesh(data) : warp_mesh_->isSelectedMesh(data);
							   },
							   [&](WarpingMesh &data, bool select) {
								   return state_ == EDIT_WARP_UV ? (select ? warp_uv_->selectMesh(data, true) : warp_uv_->deselectMesh(data, true))
								   : (select ? warp_mesh_->selectMesh(data, true) : warp_mesh_->deselectMesh(data, true));
							   },
							   [&]() {
								   auto tex = texture_source_->getTexture();
								   if(tex.isAllocated()) {
									   warping_data_->create("warp", glm::ivec2{1,1},
															 ofRectangle{0,0,tex.getWidth(),tex.getHeight()},
															 ofRectangle{0,0,tex.getWidth(),tex.getHeight()});
								   }
							   });
			PopID();
		}
		if(isStateBlending()) {
			PushID("blending");
			blending_data_->gui(
							   [&](BlendingMesh &data) {
								   return blend_editor_->isSelectedMesh(data);
							   },
							   [&](BlendingMesh &data, bool select) {
								   return select ? blend_editor_->selectMesh(data, true) : blend_editor_->deselectMesh(data, true);
							   },
							   [&]() {
								   auto tex = fbo_.getTexture();
								   if(tex.isAllocated()) {
									   blending_data_->create("blend", ofRectangle{0,0,tex.getWidth(),tex.getHeight()}, 0.9f);
								   }
							   });
			PopID();
		}
	}
	End();
	if(editor) {
		editor->gui();
	}
}

void GuiApp::exportMesh(float resample_min_interval, const std::filesystem::path &filepath, bool is_arb) const
{
	auto tex = texture_source_->getTexture();
	glm::vec2 coord_size = is_arb&&tex.isAllocated()?glm::vec2{1,1}: glm::vec2{1/tex.getWidth(), 1/tex.getHeight()};
	warping_data_->exportMesh(filepath, resample_min_interval, coord_size);
}

namespace nlohmann {
template<typename T>
struct adl_serializer<glm::tvec3<T>> {
	static void to_json(ofJson &j, const glm::tvec3<T> &v) {
		j = {v[0],v[1],v[2]};
	}
	static void from_json(const ofJson &j, glm::tvec3<T> &v) {
		v = {j[0],j[1],j[2]};
	}
};
template<>
struct adl_serializer<ofxBlendScreen::Shader::Params> {
	static void to_json(ofJson &j, const ofxBlendScreen::Shader::Params &v) {
		j = {
			{"gamma", v.gamma},
			{"luminance_control", v.luminance_control},
			{"blend_power", v.blend_power},
			{"base_color", v.base_color}
		};
	}
};
}
void GuiApp::exportMesh(const ProjectFolder &proj) const
{
	std::string folder = proj_.getExportFolder();
	bool is_arb = proj_.getIsExportMeshArb();
	{
		auto param = proj_.getExportWarpParam();
		auto tex = texture_source_->getTexture();
		glm::vec2 coord_size = is_arb&&tex.isAllocated()?glm::vec2{1,1}: glm::vec2{1/tex.getWidth(), 1/tex.getHeight()};
		warping_data_->exportMesh(ofFilePath::join(folder, param.filename), param.max_mesh_size, coord_size);
	}
	{
		auto param = proj_.getExportBlendParam();
		auto tex = fbo_.getTexture();
		glm::vec2 coord_size = is_arb&&tex.isAllocated()?glm::vec2{1,1}: glm::vec2{1/tex.getWidth(), 1/tex.getHeight()};
		blending_data_->exportMesh(ofFilePath::join(folder, param.filename), param.max_mesh_size, coord_size);
	}
	{
		auto param = proj_.getExportBlendShaderParam();
		ofSavePrettyJson(ofFilePath::join(folder, param.filename), blending_data_->getShader()->getParams());
	}
}


//--------------------------------------------------------------
void GuiApp::keyPressed(int key){
	if(ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	glm::vec2 move{0,0};
	switch(key) {
		case OF_KEY_TAB:
			if(++state_ >= NUM_STATE) {
				state_ = 0;
			}
			break;
		case OF_KEY_LEFT: move.x = -1; break;
		case OF_KEY_RIGHT: move.x = 1; break;
		case OF_KEY_UP: move.y = -1; break;
		case OF_KEY_DOWN: move.y = 1; break;
	}
	if(ImGui::IsModKeyDown(ImGuiKeyModFlags_Shift)) {
		move *= 10;
	}
	auto editor = editor_[stateName(state_)];
	if(editor && glm::length2(move) > 0) {
		editor->moveSelectedOnScreenScale(move);
	}
}

void GuiApp::mouseReleased(int x, int y, int button)
{
	if(undo_.isModified()) {
		undo_.store();
	}
}

void GuiApp::save(bool do_backup) const
{
	{
		auto pos = result_window_->getWindowPosition();
		auto size = result_window_->getWindowSize();
		proj_.setResultViewport({pos.x,pos.y,size.x,size.y});
		
		proj_.setResultEditorName(stateName(state_));
		proj_.setResultScaleToViewport(result_app_->isScaleToViewport());
		proj_.setResultShowControl(result_app_->isShowControl());
		proj_.setResultShowCursor(result_app_->isShowCursor());
	}
	proj_.setUVView(-warp_uv_->getTranslate(), warp_uv_->getScale());
	proj_.setUVGridData(warp_uv_->getGridData());
	proj_.setWarpView(-warp_mesh_->getTranslate(), warp_mesh_->getScale());
	proj_.setWarpGridData(warp_mesh_->getGridData());
	proj_.setBlendView(-blend_editor_->getTranslate(), blend_editor_->getScale());
	proj_.setBlendGridData(blend_editor_->getGridData());
	proj_.setBlendParams(blending_data_->getShader()->getParams());
	
	proj_.setBridgeResolution({fbo_.getWidth(), fbo_.getHeight()});
	
	proj_.save();

	auto filepath = proj_.getDataFilePath();
	saveDataFile(filepath);

	if(do_backup && proj_.isBackupEnabled()) {
		auto backup_path = proj_.getBackupFilePath();
		ofDirectory folder(ofFilePath::getEnclosingDirectory(backup_path));
		if(!folder.exists()) {
			folder.create();
		}
		ofFile(filepath).copyTo(backup_path);
		int num = proj_.getBackupNumLimit();
		if(num > 0) {
			folder.allowExt(ofFilePath::getFileExt(backup_path));
			folder.sortByDate();
			for(int i = 0; i < folder.size() - num; ++i) {
				folder.getFile(i).remove();
			}
		}
	}
}

void GuiApp::saveDataFile(const std::filesystem::path &filepath) const
{
	ofFile file(filepath, ofFile::WriteOnly);
	packDataFile(file);
	file.close();
}

void GuiApp::loadDataFile(const std::filesystem::path &filepath)
{
	ofFile file(filepath, ofFile::ReadOnly);
	unpackDataFile(file);
	file.close();
}

void GuiApp::packDataFile(std::ostream &stream) const
{
	SaveData saver;
	{
		glm::vec2 tex_size = proj_.getTextureSizeCache();
		warping_data_->setPackArg({1/tex_size.x, 1/tex_size.y});
		saver.append((char *)"warp", warping_data_);
	}
	{
		glm::vec2 tex_size = proj_.getBridgeResolution();
		blending_data_->setPackArg({1/tex_size.x, 1/tex_size.y});
		saver.append((char *)"blnd", blending_data_);
	}
	saver.pack(stream);
}

void GuiApp::unpackDataFile(std::istream &stream)
{
	SaveData loader;
	{
		glm::vec2 tex_size = proj_.getTextureSizeCache();
		warping_data_->setUnpackArg(tex_size);
		loader.append((char *)"warp", warping_data_);
	}
	{
		glm::vec2 tex_size = proj_.getBridgeResolution();
		blending_data_->setUnpackArg(tex_size);
		loader.append((char *)"blnd", blending_data_);
	}
	loader.unpack(stream);
}


void GuiApp::openProject(const std::filesystem::path &proj_path)
{
	proj_.WorkFolder::setRelative(proj_path);
	proj_.setup();

	{
		auto view = proj_.getResultViewport();
		result_window_->setWindowPosition(view[0], view[1]);
		result_window_->setWindowShape(view[2], view[3]);
		
		auto found = editor_.find(proj_.getResultEditorName());
		if(found != end(editor_)) {
			result_app_->setEditor(found->second);
		}
		result_app_->setScaleToViewport(proj_.isResultScaleToViewport());
		result_app_->setShowControl(proj_.isResultShowControl());
		result_app_->setShowCursor(proj_.isResultShowCursor());
	}
	{
		auto view = proj_.getUVView();
		warp_uv_->resetMatrix();
		warp_uv_->translate(view.first);
		warp_uv_->scale(view.second, {0,0});
		warp_uv_->setGridData(proj_.getUVGridData());
	}
	{
		auto view = proj_.getWarpView();
		warp_mesh_->resetMatrix();
		warp_mesh_->translate(view.first);
		warp_mesh_->scale(view.second, {0,0});
		warp_mesh_->setGridData(proj_.getWarpGridData());
	}
	{
		auto view = proj_.getBlendView();
		blend_editor_->resetMatrix();
		blend_editor_->translate(view.first);
		blend_editor_->scale(view.second, {0,0});
		blend_editor_->setGridData(proj_.getBlendGridData());
	}
	blending_data_->getShader()->getParams() = proj_.getBlendParams();
	
	updateRecent(proj_);

	if((texture_source_ = buildTextureSource(proj_))) {
		auto tex = texture_source_->getTexture();
		if(tex.isAllocated()) {
			warp_uv_->setTexture(tex);
			warp_mesh_->setTexture(tex);
		}
	}

	auto filepath = proj_.getDataFilePath();
	loadDataFile(filepath);

	auto bridge_res = proj_.getBridgeResolution();
	fbo_.allocate(bridge_res.x, bridge_res.y, GL_RGB);
	blend_editor_->setTexture(fbo_.getTexture());
	warp_mesh_->setBackgroundSize(bridge_res);
	
	initUndo();
}

void GuiApp::openRecent(int index)
{
	auto json = ofLoadJson("project_folder.json");
	auto most_recent = getJsonValue<std::vector<ofJson>>(json, "recent");
	if(index >= 0 && most_recent.size() > index) {
		auto proj_path = getJsonValue<std::string>(most_recent[index], "abs");
		openProject(proj_path);
	}
}

void GuiApp::loadRecent()
{
	auto recent = getJsonValue<ofJson>(ofLoadJson("project_folder.json"), "recent");
	recent_ = accumulate(begin(recent), end(recent), decltype(recent_){}, [](decltype(recent_) acc, const ofJson &json) {
		WorkFolder w;
		w.setAbsolute(json["abs"].get<std::string>());
		acc.push_back(w);
		return acc;
	});
}
void GuiApp::updateRecent(const ProjectFolder &proj)
{
	auto found = find_if(begin(recent_), end(recent_), [proj](const WorkFolder &w) {
		return w.toJson() == proj.WorkFolder::toJson();
	});
	if(found != end(recent_)) {
		recent_.erase(found);
	}
	recent_.push_front(proj);
	auto recent = accumulate(begin(recent_), end(recent_), std::vector<ofJson>{}, [](std::vector<ofJson> acc, const WorkFolder &w) {
		acc.push_back(w.toJson());
		return acc;
	});
	ofSavePrettyJson("project_folder.json", {{"recent", recent}});
}

void GuiApp::dragEvent(ofDragInfo dragInfo)
{
	if(dragInfo.files.empty()) return;
	auto filepath = dragInfo.files[0];
	auto ext = ofFilePath::getFileExt(filepath);
	if(ext == "maap") {
		loadDataFile(filepath);
	}
}

//--------------------------------------------------------------
void ResultView::setup()
{
	ofBackground(255,0,0);
}

void ResultView::draw()
{
	if(editor_) {
		if(is_scale_to_viewport_) {
			auto editor_size = editor_->getWorkAreaSize();
			ofScale(ofGetWidth()/editor_size.x, ofGetHeight()/editor_size.y);
		}
		editor_->drawMesh(false);
		if(is_show_control_) {
			editor_->drawControl(1);
		}
		if(is_show_cursor_) {
			editor_->drawCursor();
		}
	}
}
