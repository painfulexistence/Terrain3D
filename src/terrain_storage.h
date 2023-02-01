//© Copyright 2014-2022, Juan Linietsky, Ariel Manzur and the Godot community (CC-BY 3.0)
#ifndef TERRAINSTORAGE_CLASS_H
#define TERRAINSTORAGE_CLASS_H

#ifdef WIN32
#include <windows.h>
#endif

#include "terrain_material.h"

using namespace godot;

// TERRAIN STORAGE

class Terrain3DStorage : public Resource {
	GDCLASS(Terrain3DStorage, Resource);

	enum MapType {
		TYPE_HEIGHT,
		TYPE_CONTROL,
		TYPE_COLOR,
		TYPE_MAX
	};

	enum RegionSize {
		SIZE_64 = 64,
		SIZE_128 = 128,
		SIZE_256 = 256,
		SIZE_512 = 512,
		SIZE_1024 = 1024,
		SIZE_2048 = 2048,
	};

	struct Generated {
		RID rid = RID();
		Ref<Image> image;
		bool dirty = false;

	public:
		void clear();
		bool is_dirty() { return dirty; }
		void create(const TypedArray<Image> &p_layers);
		void create(const Ref<Image> &p_image);
		RID get_rid() { return rid; }
	};

	RegionSize region_size = SIZE_1024;
	int max_height = 512;
	const int region_map_size = 16;

	RID material;
	RID shader;
	Ref<Shader> shader_override;

	Ref<Texture2D> noise_texture;
	float noise_scale = 1.0;
	float noise_height = 0.5;
	float noise_fade = 5.0;

	TypedArray<TerrainLayerMaterial3D> layers;

	TypedArray<Vector2i> region_offsets;
	TypedArray<Image> height_maps;
	TypedArray<Image> control_maps;

	Generated generated_region_map;
	Generated generated_height_maps;
	Generated generated_control_maps;
	Generated generated_albedo_textures;
	Generated generated_normal_textures;

	bool _initialized = false;

private:
	void _update_layers();
	void _update_arrays();
	void _update_textures();
	void _update_regions();
	void _update_material();

	void _clear();
	Vector2i _get_offset_from(Vector3 p_global_position);

protected:
	static void _bind_methods();

public:
	Terrain3DStorage();
	~Terrain3DStorage();

	void set_region_size(RegionSize p_size);
	RegionSize get_region_size() const;
	void set_max_height(int p_height);
	int get_max_height() const;

	void set_layer(const Ref<TerrainLayerMaterial3D> &p_material, int p_index);
	Ref<TerrainLayerMaterial3D> get_layer(int p_index) const;
	void set_layers(const TypedArray<TerrainLayerMaterial3D> &p_layers);
	TypedArray<TerrainLayerMaterial3D> get_layers() const;
	int get_layer_count() const;

	void add_region(Vector3 p_global_position);
	void remove_region(Vector3 p_global_position);
	bool has_region(Vector3 p_global_position);
	int get_region_index(Vector3 p_global_position);
	void set_region_offsets(const TypedArray<Vector2i> &p_array);
	TypedArray<Vector2i> get_region_offsets() const;
	int get_region_count() const;

	Ref<Image> get_map(int p_region_index, MapType p_map) const;
	void force_update_maps(MapType p_map);

	void set_height_maps(const TypedArray<Image> &p_maps);
	TypedArray<Image> get_height_maps() const;
	void set_control_maps(const TypedArray<Image> &p_maps);
	TypedArray<Image> get_control_maps() const;

	RID get_material() const;
	void set_shader_override(const Ref<Shader> &p_shader);
	Ref<Shader> get_shader_override() const;

	void set_noise_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_noise_texture() const;
	void set_noise_scale(float p_scale);
	void set_noise_height(float p_height);
	void set_noise_fade(float p_fade);
	float get_noise_scale() const { return noise_scale; };
	float get_noise_height() const { return noise_height; };
	float get_noise_fade() const { return noise_fade; };
};

VARIANT_ENUM_CAST(Terrain3DStorage, MapType);
VARIANT_ENUM_CAST(Terrain3DStorage, RegionSize);

#endif // TERRAINSTORAGE_CLASS_H