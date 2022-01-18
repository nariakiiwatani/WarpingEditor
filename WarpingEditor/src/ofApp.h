#pragma once

#include "ofMain.h"
#include "ofxImGui.h"
#include "WarpingEditor.h"
#include "UVEditor.h"

class MainApp;

class GuiApp : public ofBaseApp
{
public:
	void setup();
	void update();
	void draw();
	
	void keyPressed(int key);
	void setMainApp(std::shared_ptr<MainApp> app) { main_app_ = app; }
private:
	std::shared_ptr<MainApp> main_app_;
	ofxImGui::Gui gui_;
	ofTexture texture_;
	WarpingEditor warp_;
	UVEditor uv_;
	enum State {
		EDIT_UV,
		EDIT_WRAP
	};
	int state_=EDIT_UV;
};

class MainApp : public ofBaseApp
{
public:
	void setup();
	void update();
	void draw();
	void setTexture(ofTexture texture) { texture_ = texture; }
	void setMesh(const ofMesh &mesh) { mesh_ = mesh; }
private:
	ofMesh mesh_;
	ofTexture texture_;
};
