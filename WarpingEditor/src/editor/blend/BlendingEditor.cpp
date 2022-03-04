#include "BlendingEditor.h"
#include "Quad.h"

using DataType = BlendingEditor::DataType;
using MeshType = BlendingEditor::MeshType;
using IndexType = BlendingEditor::IndexType;
using PointType = BlendingEditor::PointType;

std::vector<int> BlendingEditor::getEditableMeshIndex(int state) {
	switch(state) {
		case EDIT_FRAME: return {0};
		case EDIT_VERTEX: return {1,2};
	}
	return {0,1,2};
}

PointType BlendingEditor::getPoint(const MeshType &mesh, const IndexType &index) const
{
	return mesh.quad[index.first][index.second];
}
void BlendingEditor::moveMesh(MeshType &mesh, const glm::vec2 &delta)
{
	for(int i : getEditableMeshIndex(state_)) {
		geom::translate(mesh.quad[i], delta);
	}
}
void BlendingEditor::movePoint(MeshType &mesh, IndexType index, const glm::vec2 &delta)
{
	auto p = mesh.quad[index.first][index.second] += delta;
	if(state_ == EDIT_FRAME) {
		mesh.quad[index.first][index.second^2].x = p.x;
		mesh.quad[index.first][index.second^1].y = p.y;
	}
}
std::shared_ptr<MeshType> BlendingEditor::getMeshType(const DataType &data) const
{
	return data.mesh;
}
void BlendingEditor::forEachPoint(const DataType &data, std::function<void(const PointType&, IndexType)> func) const
{
	auto &quads = data.mesh;
	for(int i = 0; i < quads->size(); ++i) {
		auto &&q = quads->quad[i];
		for(int j = 0; j < q.size(); ++j) {
			func(q[j], {i,j});
		}
	}
}
ofMesh BlendingEditor::makeMeshFromMesh(const DataType &data, const ofColor &color) const
{
	auto tex_data = tex_.getTextureData();
	glm::vec2 tex_scale = tex_data.textureTarget == GL_TEXTURE_RECTANGLE_ARB
	? glm::vec2(1,1)
	: glm::vec2(1/tex_data.tex_w, 1/tex_data.tex_h);
	ofMesh ret = data.getMesh(tex_scale);
	auto &colors = ret.getColors();
	for(auto &&c : colors) {
		c = c*color;
	}
	return ret;
}
ofMesh BlendingEditor::makeWireFromMesh(const DataType &data, const ofColor &color) const
{
	auto tex_data = tex_.getTextureData();
	glm::vec2 tex_scale = tex_data.textureTarget == GL_TEXTURE_RECTANGLE_ARB
	? glm::vec2(1,1)
	: glm::vec2(1/fbo_.getWidth(), 1/fbo_.getHeight());
	ofMesh ret = data.getMesh(tex_scale);
	auto &colors = ret.getColors();
	for(auto &&c : colors) {
		c = c*color;
	}
	return ret;
}
ofMesh BlendingEditor::makeBackground() const
{
	return ofMesh();
}

std::shared_ptr<MeshType> BlendingEditor::getIfInside(std::shared_ptr<DataType> data, const glm::vec2 &pos, float &distance)
{
	bool found = false;
	auto &quads = data->mesh;
	auto p = getIn(pos);
	for(int i = 0; i < quads->size(); ++i) {
		auto &&q = quads->quad[i];
		if(!inside(q, p)) {
			continue;
		}
		distance = std::min(getDistanceFromCenterOfGravity(q, p), distance);
		found = true;
	}
	return found ? getMeshType(*data) : nullptr;
}

void BlendingEditor::setFboSize(glm::ivec2 size)
{
	if(!fbo_.isAllocated() || fbo_.getWidth() != size.x || fbo_.getHeight() != size.y) {
		fbo_.allocate(size.x, size.y, GL_RGBA);
	}
}

void BlendingEditor::update()
{
	Editor::update();
	fbo_.begin();
	ofClear(0);
	drawMesh(tex_);
	fbo_.end();
}

void BlendingEditor::draw() const
{
	pushScissor();
	pushMatrix();
	if(grid_.is_show) {
		drawGrid();
	}
	shader_.begin(tex_);
	drawMesh(tex_);
	shader_.end();
	drawWire();
	drawPoint(!is_enabled_hovering_uneditable_point_);
	popMatrix();
	if(is_enabled_rect_selection_) {
		drawDragRect();
	}
	popScissor();
}

void BlendingEditor::gui()
{
	using namespace ImGui;
	auto &&data = *data_;
	const auto names = std::vector<std::string>{"lt", "rt", "lb", "rb"};
	const auto quad_names = std::vector<std::string>{"frame", "outer", "inner"};

	struct GuiMesh {
		std::string label;
		std::shared_ptr<MeshType> mesh;
	};
	struct GuiPoint {
		std::string label;
		std::shared_ptr<MeshType> mesh;
		IndexType index;
	};

	float v_min[2] = {0,0};
	float v_max[2] = {tex_.getWidth(),tex_.getHeight()};
	std::vector<std::pair<std::string, std::vector<ImGui::DragScalarAsParam>>> params{
		{"px", {
			{glm::ivec2{0, tex_.getWidth()}, 1, "%d"},
			{glm::ivec2{0, tex_.getHeight()}, 1, "%d"}
		}},
		{"%", {
			{glm::vec2{0, 100}, 0.1f, "%.02f%%"}
		}},
		{"rate", {
			{glm::vec2{0, 1}, 0.001f, "%.03f"}
		}},
	};

	auto guiPoint = [&](const GuiPoint &point) {
		auto &p = point.mesh->quad[point.index.first][point.index.second];
		return DragFloatNAs(point.label, &p.x, 2, v_min, v_max, nullptr, nullptr, params, ImGuiSliderFlags_NoRoundToFormat);
	};
	auto guiMesh = [&](const GuiMesh &mesh) {
		bool ret = false;
		if(TreeNode(mesh.label.c_str())) {
			for(int i = 0; i < mesh.mesh->size(); ++i) {
				if(TreeNode(quad_names[i].c_str())) {
					auto &&q = mesh.mesh->quad[i];
					for(int j = 0; j < q.size(); ++j) {
						ret |= guiPoint({names[j], mesh.mesh, {i,j}});
					}
					TreePop();
				}
			}
			TreePop();
		}
		return ret;
	};
	std::vector<GuiMesh> meshes;
	std::vector<GuiPoint> points;

	if(Begin("Blending")) {
		if(BeginTabBar("#tab")) {
			if(BeginTabItem("selected")) {
				for(auto &&weak : op_selection_.mesh) {
					auto mesh = data.find(weak.lock());
					if(mesh.second) {
						auto m = getMeshType(*mesh.second);
						meshes.push_back({mesh.first, m});
					}
				}
				for(auto &&point : op_selection_.point) {
					auto mesh = data.find(point.first.lock());
					if(mesh.second) {
						auto m = getMeshType(*mesh.second);
						for(IndexType i : point.second) {
							points.emplace_back(GuiPoint{mesh.first+"/"+quad_names[i.first]+"/"+names[i.second], getMeshType(*mesh.second), i});
						}
					}
				}
				EndTabItem();
			}
			if(BeginTabItem("editable")) {
				for(auto &&mesh : data.getEditableData()) {
					if(mesh.second) {
						auto m = getMeshType(*mesh.second);
						meshes.push_back({mesh.first, m});
					}
				}
				EndTabItem();
			}
			if(BeginTabItem("all")) {
				for(auto &&mesh : data.getData()) {
					if(mesh.second) {
						auto m = getMeshType(*mesh.second);
						meshes.push_back({mesh.first, m});
					}
				}
				EndTabItem();
			}
			EndTabBar();
		};
		for(auto &&m : meshes) {
			if(guiMesh(m)) {
				data.find(m.mesh).second->setDirty();
			}
		}
		for(auto &&p : points) {
			if(guiPoint(p)) {
				data.find(p.mesh).second->setDirty();
			}
		}
	}
	End();
	if(Begin("Move Selected Together")) {
		auto result = gui2DPanel("panel", v_min, v_max, params);
		if(result.first) {
			moveSelected(result.second);
		}
	}
	End();
	if(Begin("Grid")) {
		Checkbox("show", &grid_.is_show);
		Checkbox("snap", &grid_.enabled_snap);
		DragFloatNAs("offset", &grid_.offset.x, 2, v_min, v_max, nullptr, nullptr, params, ImGuiSliderFlags_NoRoundToFormat);
		bool clamp_min[] = {true, true};
		DragFloatNAs("size", &grid_.size.x, 2, v_min, v_max, clamp_min, nullptr, params, ImGuiSliderFlags_NoRoundToFormat);
	}
	End();
}
