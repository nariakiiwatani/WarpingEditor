#pragma once

#include "WorkFolder.h"
#include "Editor.h"
#include "ofxBlendScreen.h"
#include "ofJson.h"

class ProjectFolder : public WorkFolder
{
public:
	struct Texture {
		enum {
			FILE, NDI
		};
		int type = FILE;
		std::string file;
		std::string ndi;
		glm::ivec2 size_cache;
	};
	struct Viewport {
		glm::vec4 result={0,0,1920,1080};
		std::pair<glm::vec2, float> uv{{0,0},1}, warp{{0,0},1}, blend{{0,0},1};
	};
	struct Export {
		std::string folder;
		bool is_arb=false;
		struct Mesh {
			std::string filename="mesh.ply";
			float max_mesh_size=100;
		} warp, blend;
		struct BlendShader {
			std::string filename="blend_shader.json";
		} blend_shader;
	};
	struct Backup {
		bool enabled=true;
		std::string folder;
		int limit=0;
	};
	struct Grid {
		EditorBase::GridData uv, warp, blend;
	};
	struct Bridge {
		glm::ivec2 resolution={1920,1080};
	};
	struct Result {
		std::string editor_name="uv";
		bool is_scale_to_viewport=false;
		bool is_show_control=false;
		bool is_show_cursor=false;
	};
public:
	void setup();
	void load();
	void save() const;
	void backup() const;
	
	std::filesystem::path getDataFilePath() const { return getAbsolute(getDataFileName()+".maap"); }
	std::string getDataFileName() const { return filename_; }
	std::string getProjFileName() const { return "project.json"; }
	
	int getTextureType() const { return texture_.type; }
	std::filesystem::path getTextureFilePath() const { return getAbsolute(texture_.file); }
	const std::string& getTextureNDIName() const { return texture_.ndi; }
	glm::ivec2 getTextureSizeCache() const { return texture_.size_cache; }

	glm::vec4 getResultViewport() const { return viewport_.result; }
	std::pair<glm::vec2, float> getUVView() const { return viewport_.uv; }
	std::pair<glm::vec2, float> getWarpView() const { return viewport_.warp; }
	std::pair<glm::vec2, float> getBlendView() const { return viewport_.blend; }

	std::string getResultEditorName() const { return result_.editor_name; }
	bool isResultScaleToViewport() const { return result_.is_scale_to_viewport; }
	bool isResultShowControl() const { return result_.is_show_control; }
	bool isResultShowCursor() const { return result_.is_show_cursor; }
	
	glm::ivec2 getBridgeResolution() const { return bridge_.resolution; }
	
	ofxBlendScreen::Shader::Params getBlendParams() const { return blend_params_; }
	
	std::string getExportFolder() const { return export_.folder; }
	bool getIsExportMeshArb() const { return export_.is_arb; }
	Export::Mesh getExportWarpParam() const { return export_.warp; }
	Export::Mesh getExportBlendParam() const { return export_.blend; }
	Export::BlendShader getExportBlendShaderParam() const { return export_.blend_shader; }
	
	bool isBackupEnabled() const { return backup_.enabled; }
	std::filesystem::path getBackupFolder() const { return getRelative(backup_.folder); }
	std::filesystem::path getBackupFilePath() const;
	int getBackupNumLimit() const { return backup_.limit; }
	
	EditorBase::GridData getUVGridData() const { return grid_.uv; }
	EditorBase::GridData getWarpGridData() const { return grid_.warp; }
	EditorBase::GridData getBlendGridData() const { return grid_.blend; }
	
	void setTextureSourceFile(const std::string &file_name);
	void setTextureSourceNDI(const std::string &ndi_name);
	void setTextureSizeCache(const glm::vec2 size) { texture_.size_cache = size; }
	
	void setResultViewport(const glm::vec4 &viewport) { viewport_.result = viewport; }
	void setUVView(const glm::vec2 &pos, float scale) { viewport_.uv = {pos, scale}; }
	void setWarpView(const glm::vec2 &pos, float scale) { viewport_.warp = {pos, scale}; }
	void setBlendView(const glm::vec2 &pos, float scale) { viewport_.blend = {pos, scale}; }
	
	void setResultEditorName(const std::string &name) { result_.editor_name = name; }
	void setResultScaleToViewport(bool enable) { result_.is_scale_to_viewport = enable; }
	void setResultShowControl(bool enable) { result_.is_show_control = enable; }
	void setResultShowCursor(bool enable) { result_.is_show_cursor = enable; }
	
	void setBridgeResolution(glm::ivec2 resolution) { bridge_.resolution = resolution; }
	
	void setBlendParams(const ofxBlendScreen::Shader::Params &params) { blend_params_ = params; }

	void setExportFolder(const std::string &folder) { export_.folder = folder; }
	void setIsExportMeshArb(bool arb) { export_.is_arb = arb; }
	void setExportWarpParam(const Export::Mesh &param) { export_.warp = param; }
	void setExportBlendParam(const Export::Mesh &param) { export_.blend = param; }
	void setExportBlendShaderParam(const Export::BlendShader &param) { export_.blend_shader = param; }

	void setUVGridData(const EditorBase::GridData &data) { grid_.uv = data; }
	void setWarpGridData(const EditorBase::GridData &data) { grid_.warp = data; }
	void setBlendGridData(const EditorBase::GridData &data) { grid_.blend = data; }
	
	void setFileName(const std::string &filename) { filename_ = filename; }
	

private:
	Texture texture_;
	Viewport viewport_;
	Export export_;
	Backup backup_;
	Grid grid_;
	Bridge bridge_;
	Result result_;
	std::string filename_;
	ofxBlendScreen::Shader::Params blend_params_;
	
	ofJson toJson() const;
	void loadJson(const ofJson &json);
};
