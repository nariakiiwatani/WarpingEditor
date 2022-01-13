//
//  Models.h
//  WarpingEditor
//
//  Created by Iwatani Nariaki on 2021/12/22.
//

#pragma once

#include <map>
#include "ofxMapperMesh.h"
#include "ofxMapperUpSampler.h"
#include "Quad.h"

class Data
{
public:
	static Data& shared() {
		static Data instance;
		return instance;
	}
	struct Mesh {
		bool is_hidden=false;
		bool is_locked=false;
		bool is_solo=false;
		std::shared_ptr<geom::Quad> uv_quad;
		std::shared_ptr<ofx::mapper::Mesh> mesh;
		std::shared_ptr<ofx::mapper::Interpolator> interpolator;
		ofMesh getMesh(float resample_min_interval, const glm::vec2 &remap_coord={1,1}) const {
			ofMesh ret = ofx::mapper::UpSampler().proc(*mesh, resample_min_interval);
			auto uv = geom::getScaled(*uv_quad, remap_coord);
			for(auto &t : ret.getTexCoords()) {
				t = geom::rescalePosition(uv, t);
			}
			return ret;
		}
		void init(const ofRectangle &rect) {
			uv_quad = std::make_shared<geom::Quad>();
			mesh = std::make_shared<ofx::mapper::Mesh>();
			mesh->init({1,1}, rect);
			interpolator = std::make_shared<ofx::mapper::Interpolator>();
			interpolator->setMesh(mesh);
			mesh->divideRow(0, 0.5f);
			mesh->divideCol(0, 0.5f);
			interpolator->selectPoint(1,1);
		}
		void update() {
			interpolator->update();
		}
	};
	std::pair<std::string, std::shared_ptr<Mesh>> create(const std::string &name="data");
	void update();
	bool remove(const std::string &name);
	bool remove(const std::shared_ptr<Mesh> mesh);
	std::map<std::string, std::shared_ptr<Mesh>>& getMesh() { return mesh_; }
	std::map<std::string, std::shared_ptr<Mesh>> getVisibleMesh();
	std::map<std::string, std::shared_ptr<Mesh>> getEditableMesh(bool include_hidden=false);
	std::shared_ptr<Mesh> find(std::shared_ptr<ofx::mapper::Mesh> mesh);
private:
	std::map<std::string, std::shared_ptr<Mesh>> mesh_;
	bool add(const std::string &name, std::shared_ptr<Mesh> mesh) {
		return mesh_.insert(std::make_pair(name, mesh)).second;
	}
};
