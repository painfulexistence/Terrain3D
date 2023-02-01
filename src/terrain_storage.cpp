//© Copyright 2014-2022, Juan Linietsky, Ariel Manzur and the Godot community (CC-BY 3.0)
#include <godot_cpp/core/class_db.hpp>

#include "terrain_logger.h"
#include "terrain_storage.h"

using namespace godot;

void Terrain3DStorage::Generated::create(const TypedArray<Image> &p_layers) {
	if (!p_layers.is_empty()) {
		rid = RenderingServer::get_singleton()->texture_2d_layered_create(p_layers, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
		dirty = false;
	} else {
		clear();
	}
}

void Terrain3DStorage::Generated::create(const Ref<Image> &p_image) {
	image = p_image;
	rid = RenderingServer::get_singleton()->texture_2d_create(image);
	dirty = false;
}

void Terrain3DStorage::Generated::clear() {
	if (rid.is_valid()) {
		RenderingServer::get_singleton()->free_rid(rid);
	}
	if (image.is_valid()) {
		image.unref();
	}
	rid = RID();
	dirty = true;
}

Terrain3DStorage::Terrain3DStorage() {
	if (!_initialized) {
		_update_material();
		_initialized = true;
	}
}

Terrain3DStorage::~Terrain3DStorage() {
	if (_initialized) {
		_clear();
	}
}

void Terrain3DStorage::_clear() {
	RenderingServer::get_singleton()->free_rid(material);
	RenderingServer::get_singleton()->free_rid(shader);

	generated_height_maps.clear();
	generated_control_maps.clear();
	generated_albedo_textures.clear();
	generated_normal_textures.clear();
	generated_region_map.clear();
}

void Terrain3DStorage::set_region_size(RegionSize p_size) {
	ERR_FAIL_COND(p_size < SIZE_64);
	ERR_FAIL_COND(p_size > SIZE_2048);

	region_size = p_size;
	RenderingServer::get_singleton()->material_set_param(material, "region_size", region_size);
	RenderingServer::get_singleton()->material_set_param(material, "region_pixel_size", 1.0f / float(region_size));
}

Terrain3DStorage::RegionSize Terrain3DStorage::get_region_size() const {
	return region_size;
}

void Terrain3DStorage::set_max_height(int p_height) {
	max_height = p_height;
	RenderingServer::get_singleton()->material_set_param(material, "terrain_height", max_height);
}

int Terrain3DStorage::get_max_height() const {
	return max_height;
}

Vector2i Terrain3DStorage::_get_offset_from(Vector3 p_global_position) {
	return Vector2i((Vector2(p_global_position.x, p_global_position.z) / float(region_size) + Vector2(0.5, 0.5)).floor());
}

void Terrain3DStorage::add_region(Vector3 p_global_position) {
	ERR_FAIL_COND(has_region(p_global_position));

	Ref<Image> hmap_img = Image::create(region_size, region_size, false, Image::FORMAT_RH);
	Ref<Image> cmap_img = Image::create(region_size, region_size, false, Image::FORMAT_RGBA8);
	Vector2i uv_offset = _get_offset_from(p_global_position);

	hmap_img->fill(Color(0.0, 0.0, 0.0, 1.0));
	cmap_img->fill(Color(0.0, 0.0, 0.0, 1.0));

	height_maps.push_back(hmap_img);
	control_maps.push_back(cmap_img);
	region_offsets.push_back(uv_offset);

	generated_height_maps.clear();
	generated_control_maps.clear();
	generated_region_map.clear();

	_update_regions();

	notify_property_list_changed();
	emit_changed();
}

void Terrain3DStorage::remove_region(Vector3 p_global_position) {
	if (get_region_count() == 1) {
		return;
	}

	int index = get_region_index(p_global_position);

	ERR_FAIL_COND_MSG(index == -1, "Map does not exist.");

	region_offsets.remove_at(index);
	height_maps.remove_at(index);
	control_maps.remove_at(index);

	generated_height_maps.clear();
	generated_control_maps.clear();
	generated_region_map.clear();

	_update_regions();

	notify_property_list_changed();
	emit_changed();
}

bool Terrain3DStorage::has_region(Vector3 p_global_position) {
	return get_region_index(p_global_position) != -1;
}

int Terrain3DStorage::get_region_index(Vector3 p_global_position) {
	Vector2i uv_offset = _get_offset_from(p_global_position);
	int index = -1;

	for (int i = 0; i < region_offsets.size(); i++) {
		Vector2i ofs = region_offsets[i];
		if (ofs == uv_offset) {
			index = i;
			break;
		}
	}
	return index;
}

void Terrain3DStorage::set_region_offsets(const TypedArray<Vector2i> &p_array) {
	region_offsets = p_array;
}

TypedArray<Vector2i> Terrain3DStorage::get_region_offsets() const {
	return region_offsets;
}

Ref<Image> Terrain3DStorage::get_map(int p_region_index, MapType p_map_type) const {
	Ref<Image> map;

	switch (p_map_type) {
		case TYPE_HEIGHT:
			map = height_maps[p_region_index];
			break;
		case TYPE_CONTROL:
			map = control_maps[p_region_index];
			break;
		case TYPE_COLOR:
			break;
		case TYPE_MAX:
			break;
		default:
			break;
	}
	return map;
}

void Terrain3DStorage::force_update_maps(MapType p_map_type) {
	switch (p_map_type) {
		case TYPE_HEIGHT:
			generated_height_maps.clear();
			break;
		case TYPE_CONTROL:
			generated_control_maps.clear();
			break;
		case TYPE_COLOR:
			break;
		case TYPE_MAX:
		default:
			generated_height_maps.clear();
			generated_control_maps.clear();
			break;
	}
	_update_regions();
}

void Terrain3DStorage::set_height_maps(const TypedArray<Image> &p_maps) {
	height_maps = p_maps;
	force_update_maps(TYPE_HEIGHT);
}

TypedArray<Image> Terrain3DStorage::get_height_maps() const {
	return height_maps;
}

void Terrain3DStorage::set_control_maps(const TypedArray<Image> &p_maps) {
	control_maps = p_maps;
	force_update_maps(TYPE_CONTROL);
}

TypedArray<Image> Terrain3DStorage::get_control_maps() const {
	return control_maps;
}

int Terrain3DStorage::get_region_count() const {
	return region_offsets.size();
}

RID Terrain3DStorage::get_material() const {
	return material;
}

void Terrain3DStorage::set_shader_override(const Ref<Shader> &p_shader) {
	shader_override = p_shader;
}

Ref<Shader> Terrain3DStorage::get_shader_override() const {
	return shader_override;
}

void Terrain3DStorage::set_noise_texture(const Ref<Texture2D> &p_texture) {
	noise_texture = p_texture;
	RID rid = noise_texture.is_valid() ? noise_texture->get_rid() : RID();
	RenderingServer::get_singleton()->material_set_param(material, "noise", rid);
	_update_material();
}

Ref<Texture2D> Terrain3DStorage::get_noise_texture() const {
	return noise_texture;
}

void Terrain3DStorage::set_noise_scale(float p_scale) {
	noise_scale = p_scale;
	RenderingServer::get_singleton()->material_set_param(material, "noise_scale", noise_scale);
}

void Terrain3DStorage::set_noise_height(float p_height) {
	noise_height = p_height;
	RenderingServer::get_singleton()->material_set_param(material, "noise_height", noise_height);
}

void Terrain3DStorage::set_noise_fade(float p_fade) {
	noise_fade = p_fade;
	RenderingServer::get_singleton()->material_set_param(material, "noise_fade", noise_fade);
}

void Terrain3DStorage::set_layer(const Ref<TerrainLayerMaterial3D> &p_material, int p_index) {
	if (p_index < layers.size()) {
		if (p_material.is_null()) {
			Ref<TerrainLayerMaterial3D> material_to_remove = layers[p_index];
			material_to_remove->disconnect("texture_changed", Callable(this, "_update_textures"));
			material_to_remove->disconnect("value_changed", Callable(this, "_update_arrays"));
			layers.remove_at(p_index);
		} else {
			layers[p_index] = p_material;
		}
	} else {
		layers.push_back(p_material);
	}
	generated_albedo_textures.clear();
	generated_normal_textures.clear();

	_update_layers();
	notify_property_list_changed();
}

Ref<TerrainLayerMaterial3D> Terrain3DStorage::get_layer(int p_index) const {
	return layers[p_index];
}

void Terrain3DStorage::set_layers(const TypedArray<TerrainLayerMaterial3D> &p_layers) {
	layers = p_layers;
	generated_albedo_textures.clear();
	generated_normal_textures.clear();
	_update_layers();
}

TypedArray<TerrainLayerMaterial3D> Terrain3DStorage::get_layers() const {
	return layers;
}

int Terrain3DStorage::get_layer_count() const {
	return layers.size();
}

void Terrain3DStorage::_update_layers() {
	LOG(INFO, "Generating material layers");
	for (int i = 0; i < layers.size(); i++) {
		Ref<TerrainLayerMaterial3D> l_material = layers[i];

		if (!l_material->is_connected("texture_changed", Callable(this, "_update_textures"))) {
			l_material->connect("texture_changed", Callable(this, "_update_textures"));
		}
		if (!l_material->is_connected("value_changed", Callable(this, "_update_values"))) {
			l_material->connect("value_changed", Callable(this, "_update_values"));
		}
	}
	_update_arrays();
	_update_textures();
}

void Terrain3DStorage::_update_arrays() {
	LOG(INFO, "Generating terrain color and scale arrays");
	PackedVector3Array uv_scales;
	PackedColorArray colors;

	for (int i = 0; i < layers.size(); i++) {
		Ref<TerrainLayerMaterial3D> l_material = layers[i];

		uv_scales.push_back(l_material->get_uv_scale());
		colors.push_back(l_material->get_albedo());
	}

	emit_changed();
}

void Terrain3DStorage::_update_textures() {
	if (generated_albedo_textures.is_dirty()) {
		LOG(INFO, "Generating terrain albedo arrays");
		Array albedo_texture_array;
		for (int i = 0; i < layers.size(); i++) {
			Ref<TerrainLayerMaterial3D> l_material = layers[i];
			albedo_texture_array.push_back(l_material->get_albedo_texture());
		}
		generated_albedo_textures.create(albedo_texture_array);
	}

	if (generated_normal_textures.is_dirty()) {
		LOG(INFO, "Generating terrain normal arrays");
		Array normal_texture_array;
		for (int i = 0; i < layers.size(); i++) {
			Ref<TerrainLayerMaterial3D> l_material = layers[i];
			normal_texture_array.push_back(l_material->get_normal_texture());
		}
		generated_normal_textures.create(normal_texture_array);
	}
}

void Terrain3DStorage::_update_regions() {
	if (generated_height_maps.is_dirty()) {
		LOG(INFO, "Updating height maps");
		generated_height_maps.create(height_maps);
	}
	if (generated_control_maps.is_dirty()) {
		LOG(INFO, "Updating control maps");
		generated_control_maps.create(control_maps);
	}
	if (generated_region_map.is_dirty()) {
		LOG(INFO, "Updating region map");

		Ref<Image> image = Image::create(region_map_size, region_map_size, false, Image::FORMAT_RG8);
		image->fill(Color(0.0, 0.0, 0.0, 1.0));

		for (int i = 0; i < region_offsets.size(); i++) {
			Vector2i ofs = region_offsets[i];

			Color col = Color(float(i + 1) / 255.0, 1.0, 0, 1);
			image->set_pixelv(ofs + (Vector2i(region_map_size, region_map_size) / 2), col);
		}

		generated_region_map.create(image);
	}
	RenderingServer::get_singleton()->material_set_param(material, "height_maps", generated_height_maps.get_rid());
	RenderingServer::get_singleton()->material_set_param(material, "control_maps", generated_control_maps.get_rid());
	RenderingServer::get_singleton()->material_set_param(material, "region_map", generated_region_map.get_rid());
	RenderingServer::get_singleton()->material_set_param(material, "region_map_size", region_map_size);
}

void Terrain3DStorage::_update_material() {
	LOG(INFO, "Updating material");

	if (!material.is_valid()) {
		material = RenderingServer::get_singleton()->material_create();
	}

	if (!shader.is_valid()) {
		shader = RenderingServer::get_singleton()->shader_create();
	}

	bool use_noise = noise_texture.is_valid();

	{
		String code = "shader_type spatial;\n";
		code += "render_mode depth_draw_opaque, diffuse_burley;\n";
		code += "\n";

		// Uniforms
		code += "uniform float terrain_height = 512.0;\n";
		code += "uniform float region_size = 1024.0;\n";
		code += "uniform float region_pixel_size = 1.0;\n";
		code += "uniform int region_map_size = 16;\n";
		code += "\n";

		code += "uniform sampler2D region_map : filter_linear, repeat_disable, hint_default_black;\n";
		code += "uniform sampler2DArray height_maps : filter_linear_mipmap, repeat_enable;\n";
		code += "uniform sampler2DArray control_maps : filter_linear_mipmap, repeat_enable;\n";
		code += "\n";

		// For some reason 'hint_default_black' doesn't work when shader is build from code.
		if (use_noise) {
			code += "uniform sampler2D noise : filter_linear_mipmap_anisotropic, hint_default_black;\n";
			code += "uniform float noise_scale = 1.0;\n";
			code += "uniform float noise_height = 0.5;\n";
			code += "uniform float noise_fade = 5.0;\n";
		}

		code += "uniform sampler2DArray texture_array_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;\n";
		code += "uniform sampler2DArray texture_array_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;\n";
		code += "\n";
		code += "uniform vec3 texture_uv_scale_array[256];\n";
		code += "uniform vec3 texture_3d_projection_array[256];\n";
		code += "uniform vec4 texture_color_array[256];\n";
		code += "uniform int texture_array_normal_max;\n";
		code += "\n";

		// Functions
		code += "vec3 unpack_normal(vec4 rgba) {\n";
		code += "	vec3 n = rgba.xzy * 2.0 - vec3(1.0);\n";
		code += "	n.z *= -1.0;\n";
		code += "	return n;\n";
		code += "}\n\n";

		code += "vec4 pack_normal(vec3 n, float a) {\n";
		code += "	n.z *= -1.0;\n";
		code += "	return vec4((n.xzy + vec3(1.0)) * 0.5, a);\n";
		code += "}\n\n";

		code += "float get_height(vec2 uv) {\n";
		code += "	float index = floor(texelFetch(region_map, ivec2(floor(uv))+(region_map_size/2), 0).r * 255.0);\n";

		if (use_noise) {
			code += "	float weight = texture(region_map, (uv/float(region_map_size))+0.5).g;\n";
		}

		code += "	float height = 0.0;\n";
		code += "	if (index > 0.0){\n";
		code += "		height = texture(height_maps, vec3(uv, index - 1.0)).r;\n";
		code += "	}\n";

		if (use_noise) {
			code += "	height = mix(height, texture(noise, uv * noise_scale).r * noise_height, pow(1.0-weight, noise_fade) );\n";
		}

		code += "	return height * terrain_height;\n";
		code += "}\n\n";

		code += "vec3 get_normal(vec2 uv) {\n";
		code += "	float left = get_height(uv + vec2(-region_pixel_size, 0));\n";
		code += "	float right = get_height(uv + vec2(region_pixel_size, 0));\n";
		code += "	float back = get_height(uv + vec2(0, -region_pixel_size));\n";
		code += "	float fore = get_height(uv + vec2(0, region_pixel_size));\n";
		code += "\n";
		code += "	vec3 horizontal = vec3(2.0, right - left, 0.0);\n";
		code += "	vec3 vertical = vec3(0.0, back - fore, 2.0);\n";
		code += "	vec3 normal = normalize(cross(vertical, horizontal));\n";
		code += "	normal.z *= -1.0;\n";
		code += "	return normal;\n";
		code += "}\n\n";

		// Vertex Shader
		code += "void vertex() {\n";
		code += "	vec3 world_vertex = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;\n";
		code += "	UV2 = (world_vertex.xz / vec2(region_size)) + vec2(0.5);\n";
		code += "	UV = world_vertex.xz * 0.5;\n";
		code += "	VERTEX.y = get_height(UV2);\n";
		code += "\n";
		code += "	NORMAL = vec3(0, 1, 0);\n";
		code += "	TANGENT = cross(NORMAL, vec3(0, 0, 1));\n";
		code += "	BINORMAL = cross(NORMAL, TANGENT);\n";
		code += "}\n\n";

		// Fragment Shader
		code += "void fragment() {\n";
		code += "	vec3 normal = vec3(0.5, 0.5, 1.0);\n";
		code += "	vec3 color = vec3(0.0);\n";
		code += "	float rough = 1.0;\n";
		code += "\n";
		code += "	NORMAL = mat3(VIEW_MATRIX) * get_normal(UV2);\n";
		code += "\n";
		code += "	vec2 p = UV * 4.0;\n";
		code += "	vec2 ddx = dFdx(p);\n";
		code += "	vec2 ddy = dFdy(p);\n";
		code += "	vec2 w = max(abs(ddx), abs(ddy)) + 0.01;\n";
		code += "	vec2 i = 2.0 * (abs(fract((p - 0.5 * w) / 2.0) - 0.5) - abs(fract((p + 0.5 * w) / 2.0) - 0.5)) / w;\n";
		code += "	color = vec3((0.5 - 0.5 * i.x * i.y) * 0.2 + 0.2);\n";
		code += "\n";
		code += "	ALBEDO = color;\n";
		code += "	ROUGHNESS = rough;\n";
		code += "	NORMAL_MAP = normal;\n";
		code += "	NORMAL_MAP_DEPTH = 1.0;\n";
		code += "\n";
		code += "}\n\n";

		String string_code = String(code);

		RenderingServer::get_singleton()->shader_set_code(shader, string_code);
		RenderingServer::get_singleton()->material_set_shader(material, shader_override.is_null() ? shader : shader_override->get_rid());
	}
	set_region_size(region_size);
}

void Terrain3DStorage::_bind_methods() {
	BIND_ENUM_CONSTANT(TYPE_HEIGHT);
	BIND_ENUM_CONSTANT(TYPE_CONTROL);
	BIND_ENUM_CONSTANT(TYPE_COLOR);
	BIND_ENUM_CONSTANT(TYPE_MAX);

	BIND_ENUM_CONSTANT(SIZE_64);
	BIND_ENUM_CONSTANT(SIZE_128);
	BIND_ENUM_CONSTANT(SIZE_256);
	BIND_ENUM_CONSTANT(SIZE_512);
	BIND_ENUM_CONSTANT(SIZE_1024);
	BIND_ENUM_CONSTANT(SIZE_2048);

	ClassDB::bind_method(D_METHOD("set_region_size", "size"), &Terrain3DStorage::set_region_size);
	ClassDB::bind_method(D_METHOD("get_region_size"), &Terrain3DStorage::get_region_size);
	ClassDB::bind_method(D_METHOD("set_max_height", "height"), &Terrain3DStorage::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &Terrain3DStorage::get_max_height);

	ClassDB::bind_method(D_METHOD("set_shader_override", "shader"), &Terrain3DStorage::set_shader_override);
	ClassDB::bind_method(D_METHOD("get_shader_override"), &Terrain3DStorage::get_shader_override);

	ClassDB::bind_method(D_METHOD("set_noise_texture", "texture"), &Terrain3DStorage::set_noise_texture);
	ClassDB::bind_method(D_METHOD("get_noise_texture"), &Terrain3DStorage::get_noise_texture);
	ClassDB::bind_method(D_METHOD("set_noise_scale", "scale"), &Terrain3DStorage::set_noise_scale);
	ClassDB::bind_method(D_METHOD("get_noise_scale"), &Terrain3DStorage::get_noise_scale);
	ClassDB::bind_method(D_METHOD("set_noise_height", "height"), &Terrain3DStorage::set_noise_height);
	ClassDB::bind_method(D_METHOD("get_noise_height"), &Terrain3DStorage::get_noise_height);
	ClassDB::bind_method(D_METHOD("set_noise_fade", "fade"), &Terrain3DStorage::set_noise_fade);
	ClassDB::bind_method(D_METHOD("get_noise_fade"), &Terrain3DStorage::get_noise_fade);

	ClassDB::bind_method(D_METHOD("set_layer", "material", "index"), &Terrain3DStorage::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer", "index"), &Terrain3DStorage::get_layer);
	ClassDB::bind_method(D_METHOD("set_layers", "layers"), &Terrain3DStorage::set_layers);
	ClassDB::bind_method(D_METHOD("get_layers"), &Terrain3DStorage::get_layers);

	ClassDB::bind_method(D_METHOD("add_region", "global_position"), &Terrain3DStorage::add_region);
	ClassDB::bind_method(D_METHOD("remove_region", "global_position"), &Terrain3DStorage::remove_region);
	ClassDB::bind_method(D_METHOD("has_region", "global_position"), &Terrain3DStorage::has_region);
	ClassDB::bind_method(D_METHOD("get_region_index", "global_position"), &Terrain3DStorage::get_region_index);
	ClassDB::bind_method(D_METHOD("force_update_maps", "map_type"), &Terrain3DStorage::force_update_maps);
	ClassDB::bind_method(D_METHOD("get_map", "region_index", "map_type"), &Terrain3DStorage::get_map);

	ClassDB::bind_method(D_METHOD("set_height_maps", "maps"), &Terrain3DStorage::set_height_maps);
	ClassDB::bind_method(D_METHOD("get_height_maps"), &Terrain3DStorage::get_height_maps);
	ClassDB::bind_method(D_METHOD("set_control_maps", "maps"), &Terrain3DStorage::set_control_maps);
	ClassDB::bind_method(D_METHOD("get_control_maps"), &Terrain3DStorage::get_control_maps);
	ClassDB::bind_method(D_METHOD("set_region_offsets", "offsets"), &Terrain3DStorage::set_region_offsets);
	ClassDB::bind_method(D_METHOD("get_region_offsets"), &Terrain3DStorage::get_region_offsets);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "region_size", PROPERTY_HINT_ENUM, "64:64, 128:128, 256:256, 512:512, 1024:1024, 2048:2048"), "set_region_size", "get_region_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_height", PROPERTY_HINT_RANGE, "1, 1024, 1"), "set_max_height", "get_max_height");

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "height_maps", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "Image")), "set_height_maps", "get_height_maps");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "control_maps", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "Image")), "set_control_maps", "get_control_maps");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "region_offsets", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::VECTOR2, PROPERTY_HINT_NONE)), "set_region_offsets", "get_region_offsets");

	ADD_GROUP("Noise", "noise_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_noise_texture", "get_noise_texture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_scale", PROPERTY_HINT_RANGE, "0.0, 1.0"), "set_noise_scale", "get_noise_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_height", PROPERTY_HINT_RANGE, "0.0, 1.0"), "set_noise_height", "get_noise_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_fade", PROPERTY_HINT_RANGE, "0.1, 10.0"), "set_noise_fade", "get_noise_fade");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shader_override", PROPERTY_HINT_RESOURCE_TYPE, "Shader"), "set_shader_override", "get_shader_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "layers", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "TerrainLayerMaterial3D")), "set_layers", "get_layers");
}
