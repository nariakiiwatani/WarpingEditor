#include "ofApp.h"
#include "Models.h"
#include "GuiFunc.h"
#include "Icon.h"
#include "ImGuiFileDialog.h"

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

	texture_source_.loadFromFile("testdata/of.png");
	auto tex = texture_source_.getTexture();
	main_app_->setTexture(tex);
	uv_.setTexture(tex);
	warp_.setTexture(tex);

	auto &data = Data::shared();
	data.create("mesh", {0,0,tex.getWidth(),tex.getHeight()});
}

//--------------------------------------------------------------
void GuiApp::update(){
	texture_source_.update();
	if(texture_source_.isFrameNew()) {
		auto tex = texture_source_.getTexture();
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
		auto tex = texture_source_.getTexture();
		main_app_->setMesh(data.getMeshForExport(100, ofGetUsingArbTex() ? glm::vec2{tex.getWidth(), tex.getHeight()} : glm::vec2{1,1}));
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
	if(BeginMainMenuBar()) {
		if(BeginMenu("Texture")) {
			if(MenuItem("Change Texture...")) {
				ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", "{.png,.gif,.jpg,.jpeg,.mov,.mp4}", ofToDataPath("."));
			}
			EndMenu();
		}
		/*
		if(BeginMenu("NDI")) {
			auto source = ndi_finder_.getSources();
			for(auto &&s : source) {
				std::stringstream ss;
				ss << s.p_ndi_name << "(" << s.p_url_address << ")";
				if(MenuItem(ss.str().c_str())) {
					texture_source_.setupNDI(s);
				}
			}
			EndMenu();
		}
		 */
		EndMainMenuBar();
	}
	if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk() == true) {
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
			if(texture_source_.loadFromFile(filePathName)) {
				auto tex = texture_source_.getTexture();
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
			auto tex = texture_source_.getTexture();
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

//--------------------------------------------------------------
void GuiApp::keyPressed(int key){
	switch(key) {
		case OF_KEY_TAB:
			state_ ^= 1;
			break;
		case 's': Data::shared().save("saved.bin"); break;
		case 'l': Data::shared().load("saved.bin"); break;
		case 'e': {
			auto tex = texture_source_.getTexture();
			Data::shared().exportMesh("export.ply", 100, {1,1});
			Data::shared().exportMesh("export_arb.ply", 100, {tex.getWidth(), tex.getHeight()});
		}	break;
	}
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
