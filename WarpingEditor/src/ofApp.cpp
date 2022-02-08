#include "ofApp.h"
#include "Models.h"
#include "GuiFunc.h"
#include "Icon.h"
#include "ImGuiFileDialog.h"

namespace {
template<typename T>
auto getJsonValue(const ofJson &json, const std::string &key, T default_value={}) -> T {
	return json.contains(key) ? json[key].get<T>() : default_value;
}
}

//--------------------------------------------------------------
void GuiApp::setup(){
	ofDisableArbTex();
	Icon::init();
	
	ofEnableArbTex();
	ofBackground(0);

	gui_.setup(nullptr, true, ImGuiConfigFlags_DockingEnable, true);
	
	ndi_finder_.watchSources();
	
	uv_.setup();
	warp_.setup();
	
	loadRecent();
	openRecent();
}

//--------------------------------------------------------------
void GuiApp::update(){
	texture_source_->update();
	if(texture_source_->isFrameNew()) {
		auto tex = texture_source_->getTexture();
		main_app_->setTexture(tex);
		uv_.setTexture(tex);
		warp_.setTexture(tex);
	}
	bool gui_focused = ImGui::GetIO().WantCaptureMouse;
	uv_.setEnableViewportEditByMouse(!gui_focused);
	uv_.setEnableMeshEditByMouse(!gui_focused);
	warp_.setEnableViewportEditByMouse(!gui_focused);
	warp_.setEnableMeshEditByMouse(!gui_focused);
	bool update_mesh = true;
	switch(state_) {
		case EDIT_UV:
			uv_.setRegion(ofGetCurrentViewport());
			uv_.update();
			update_mesh = !uv_.isPreventMeshInterpolation();
			break;
		case EDIT_WRAP:
			warp_.setRegion(ofGetCurrentViewport());
			warp_.update();
			update_mesh = !warp_.isPreventMeshInterpolation();
			break;
	}
	if(update_mesh) {
		auto &data = Data::shared();
		data.update();
		auto tex = texture_source_->getTexture();
		auto tex_data = tex.getTextureData();
		glm::vec2 tex_size{tex_data.tex_w, tex_data.tex_h};
		glm::vec2 tex_uv = tex_data.textureTarget == GL_TEXTURE_RECTANGLE_ARB
		? glm::vec2{tex_data.tex_w, tex_data.tex_h}
		: glm::vec2{tex_data.tex_t, tex_data.tex_u};
		main_app_->setMesh(data.getMeshForExport(100, tex_uv));
	}
}

//--------------------------------------------------------------
void GuiApp::draw(){
	switch(state_) {
		case EDIT_UV:
			uv_.draw();
			break;
		case EDIT_WRAP:
			warp_.draw();
			break;
	}
	
	gui_.begin();
	using namespace ImGui;
	bool is_open_export_dialog_trigger_=false;
	if(BeginMainMenuBar()) {
		if(BeginMenu("Project")) {
			if(MenuItem("Open file...", "Ctrl+O")) {
				ImGuiFileDialog::Instance()->OpenDialog("ChooseProjDlgKey", "Choose Project Folder", nullptr, ofToDataPath(""));
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
				EndMenu();
			}
			Separator();
			if(MenuItem("Save", "Ctrl+S")) {
				save();
			}
			if(MenuItem("Save as...", "Ctrl+Shift+S")) {
				ImGuiFileDialog::Instance()->OpenDialog("SaveProjDlgKey", "Save Project Folder", nullptr, ofToDataPath(""));
			}
			EndMenu();
		}
		if(BeginMenu("Texture")) {
			if(BeginMenu("Image File")) {
				if(MenuItem("Load from file...")) {
					ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", "{.png,.gif,.jpg,.jpeg,.mov,.mp4}", ofFilePath::addTrailingSlash(proj_.getAbsolute().string()));
				}
				Separator();
				std::string filepath;
				if(SelectFileMenu(proj_.getRelative().string(), filepath, true, {"png","gif","jpg","jpeg","mov","mp4"})) {
					filepath = ofToDataPath(filepath, true);
					proj_.setTextureSource("File", filepath);
					if((texture_source_ = proj_.buildTextureSource())) {
						auto tex = texture_source_->getTexture();
						main_app_->setTexture(tex);
						uv_.setTexture(tex);
						warp_.setTexture(tex);
					}
				}
				EndMenu();
			}
			if(BeginMenu("NDI")) {
				auto source = ndi_finder_.getSources();
				for(auto &&s : source) {
					std::stringstream ss;
					ss << s.ndi_name << "(" << s.url_address << ")";
					if(MenuItem(ss.str().c_str())) {
						proj_.setTextureSource("NDI", s.ndi_name);
						texture_source_ = proj_.buildTextureSource();
					}
				}
				EndMenu();
			}
			EndMenu();
		}
		if(BeginMenu("Export")) {
			if(MenuItem("export by recent settings", "Ctrl+E")) {
				exportMesh(proj_);
			}
			if(MenuItem("export...")) {
				is_open_export_dialog_trigger_ = true;
			}
			EndMenu();
		}
		EndMainMenuBar();
	}

	if(ImGuiFileDialog::Instance()->Display("SelectFolderDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			proj_.setExportFolder(filePathName);
			is_open_export_dialog_trigger_ = true;
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(is_open_export_dialog_trigger_) {
		OpenPopup("is_open_export_dialog_trigger_");
	}
	if(BeginPopup("is_open_export_dialog_trigger_")) {
		std::string folder = proj_.getExportFolder();
		float resample_min_interval = proj_.getExportMeshMinInterval();
		bool is_arb = proj_.getIsExportMeshArb();
		std::string filename = proj_.getExportFileName();
		if(InputFloat("resample_min_interval", &resample_min_interval)) {
			proj_.setExportMeshMinInterval(resample_min_interval);
		}
		if(Checkbox("arb", &is_arb)) {
			proj_.setIsExportMeshArb(is_arb);
		}
		if(EditText("export folder", folder, 1024)) {
			proj_.setExportFolder(folder);
		}
		SameLine();
		if(Button("...")) {
			ImGuiFileDialog::Instance()->OpenDialog("SelectFolderDlgKey", "Choose Export Folder", nullptr, folder);
		}
		if(EditText("filename", filename)) {
			proj_.setExportFileName(filename);
		}
		if(Button("export")) {
			exportMesh(proj_);
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
			proj_.setTextureSource("File", filePathName);
			if((texture_source_ = proj_.buildTextureSource())) {
				auto tex = texture_source_->getTexture();
				main_app_->setTexture(tex);
				uv_.setTexture(tex);
				warp_.setTexture(tex);
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}
	if(Begin("MainWindow")) {
		auto position = main_window_->getWindowPosition();
		if(DragFloat2("position", &position.x, 1, 0, -1, "%0.0f")) {
			main_window_->setWindowPosition(position.x, position.y);
		}
		auto size = main_window_->getWindowSize();
		if(DragFloat2("size", &size.x, 1, 1, -1, "%0.0f")) {
			main_window_->setWindowShape(size.x, size.y);
		}
	}
	End();
	if(Begin("Switch")) {
		auto &data = Data::shared();
		for(auto &&m : data.getMesh()) {
			PushID(m.first.c_str());
			ToggleButton("##hide", m.second->is_hidden, Icon::HIDE, Icon::SHOW, {17,17}, 0);	SameLine();
			ToggleButton("##lock", m.second->is_locked, Icon::LOCK, Icon::UNLOCK, {17,17}, 0);	SameLine();
			ToggleButton("##solo", m.second->is_solo, Icon::FLAG, Icon::BLANK, {17,17}, 0);	SameLine();
			Selectable(m.first.c_str());
			PopID();
		}
		if(Button("create new")) {
			auto tex = texture_source_->getTexture();
			data.create("mesh", {0,0,tex.getWidth(),tex.getHeight()});
		}
	}
	End();
	switch(state_) {
		case EDIT_UV:
			uv_.gui();
			break;
		case EDIT_WRAP:
			warp_.gui();
			break;
	}
	gui_.end();
}

void GuiApp::exportMesh(float resample_min_interval, const std::filesystem::path &filepath, bool is_arb) const
{
	auto tex = texture_source_->getTexture();
	glm::vec2 coord_size = is_arb?glm::vec2{tex.getWidth(), tex.getHeight()}:glm::vec2{1,1};
	Data::shared().exportMesh(filepath, resample_min_interval, coord_size);
}
void GuiApp::exportMesh(const ProjectFolder &proj) const
{
	std::string folder = proj_.getExportFolder();
	float resample_min_interval = proj_.getExportMeshMinInterval();
	bool is_arb = proj_.getIsExportMeshArb();
	std::string filename = proj_.getExportFileName();
	exportMesh(resample_min_interval, ofFilePath::join(folder, filename), is_arb);
}


//--------------------------------------------------------------
void GuiApp::keyPressed(int key){
	if(ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	glm::vec2 move{0,0};
	switch(key) {
		case OF_KEY_TAB:
			state_ ^= 1;
			break;
		case OF_KEY_LEFT: move.x = -1; break;
		case OF_KEY_RIGHT: move.x = 1; break;
		case OF_KEY_UP: move.y = -1; break;
		case OF_KEY_DOWN: move.y = 1; break;
	}
	if(ImGui::IsModKeyDown(ImGuiKeyModFlags_Shift)) {
		move *= 10;
	}
	switch(state_) {
		case EDIT_UV:
			uv_.moveSelectedOnScreenScale(move);
			break;
		case EDIT_WRAP:
			warp_.moveSelectedOnScreenScale(move);
			break;
	}
}

void GuiApp::save(bool do_backup) const
{
	{
		auto pos = main_window_->getWindowPosition();
		auto size = main_window_->getWindowSize();
		proj_.setMainViewport({pos.x,pos.y,size.x,size.y});
	}
	proj_.setUVView(-uv_.getTranslate(), uv_.getScale());
	proj_.setWarpView(-warp_.getTranslate(), warp_.getScale());
	proj_.save();
	
	if(do_backup) {
		proj_.backup();
	}
}

void GuiApp::openProject(const std::filesystem::path &proj_path)
{
	proj_.WorkFolder::setRelative(proj_path);
	proj_.setup();

	if((texture_source_ = proj_.buildTextureSource())) {
		auto tex = texture_source_->getTexture();
		main_app_->setTexture(tex);
		uv_.setTexture(tex);
		warp_.setTexture(tex);
	}
	{
		auto view = proj_.getMainViewport();
		main_window_->setWindowPosition(view.x, view.y);
		main_window_->setWindowShape(view.width, view.height);
	}
	{
		auto view = proj_.getUVView();
		uv_.resetMatrix();
		uv_.translate(view.first);
		uv_.scale(view.second, {0,0});
	}
	{
		auto view = proj_.getWarpView();
		warp_.resetMatrix();
		warp_.translate(view.first);
		warp_.scale(view.second, {0,0});
	}
	proj_.load();
	
	updateRecent(proj_);
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
		return w.toJson() == proj.toJson();
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

//--------------------------------------------------------------
void MainApp::setup()
{
	ofBackground(0);
}
void MainApp::update()
{
}
void MainApp::draw()
{
	texture_.bind();
	mesh_.draw();
	texture_.unbind();
}
