#include <iostream>
#include <fstream>
#include <memory>
#include <cstring>
#include <vector>
#include <cmath>
#include <cstdio>
#include "glm/glm.hpp"

using byte = unsigned char;

#define randf (std::rand() / static_cast<float>(RAND_MAX))

constexpr int W = 1920;
constexpr int H = 1080;
constexpr int LEN = W * H;
constexpr float SQRT3_INV = 0.57735f;
constexpr float SHADOW_DARKNESS = .2f;
constexpr size_t SKYBOX_W = 3000;
constexpr size_t SKYBOX_H = 3000;

#define LIGHT_DIR { SQRT3_INV, SQRT3_INV, SQRT3_INV }

struct Pixel {
	byte b, g, r;
	void from_color(const glm::vec3 &v) {
		r = static_cast<byte>(v.x * 255.f + 0.5f);
		g = static_cast<byte>(v.y * 255.f + 0.5f);
		b = static_cast<byte>(v.z * 255.f + 0.5f);
	}
};

enum ObjectType {
	SPHERE,
	PLANE
};

struct Object {
	ObjectType type;
	glm::vec3 color;
	float reflectance;
	union {
		struct {
			glm::vec3 p;
			glm::vec3 n;
		} as_plane;
		struct {
			glm::vec3 center;
			float radius;
		} as_sphere;
	};
};

static glm::vec3 skybox[SKYBOX_W * SKYBOX_H];

static_assert(sizeof(Pixel) == 3, "pixel not 3 bytes");

static bool hittest(const Object &o, const glm::vec3 &start, const glm::vec3 &dir, float &dist, glm::vec3 &point, glm::vec3 &normal)
{
	if (o.type == SPHERE) {
		const float radius = o.as_sphere.radius;
		const glm::vec3 &center = o.as_sphere.center;
		const glm::vec3 T = center - start;
		const float d = glm::dot(dir, T);
		const float disc = d*d - glm::dot(T, T) + radius*radius;
		if (disc < 0.f)
			return false;
		dist = d - std::sqrt(disc);
		if (dist < 0.)
			return false;
		point = start + dist * dir;
		normal = glm::normalize(center - point);
		return true;
	} else {
		const glm::vec3 &p = o.as_plane.p;
		const glm::vec3 &n = o.as_plane.n;
		const float deno = glm::dot(dir, n);
		if (std::fabs(deno) <= .00001f)
			return false;
		const float t = glm::dot((p - start), n) / deno;
		if (t < 0.)
			return false;
		const glm::vec3 m = dir * t;
		point = start + m;
		normal = n;
		dist = glm::length(m);
		return true;
	}
}

static std::vector<Object> generate_objects()
{
	std::vector<Object> objects(5);
	{
		Object &o = objects[0];
		o.type = PLANE;
		o.as_plane.p = { 0.f, 5.f, 0.f };
		o.as_plane.n = { 0.f, 1.f, 0.f };
		o.color = { 0.f, .78f, .3f };
		o.reflectance = .2f;
	}
	{
		Object &o = objects[1];
		o.type = SPHERE;
		o.as_sphere.center = { 3.f, -1.f, 6.f };
		o.as_sphere.radius = 1.5f;
		o.color = { .9f, .9f, .3f };
		o.reflectance = .5f;
	}
	{
		Object &o = objects[2];
		o.type = SPHERE;
		o.as_sphere.center = { 0.f, 1.f, 5.f };
		o.as_sphere.radius = 1.5f;
		o.color = { 0.39215f, 0.58431f, 0.92941f };
		o.reflectance = .5f;
	}
	{
		Object &o = objects[3];
		o.type = SPHERE;
		o.as_sphere.center = { -10.f, 3.5f, 20.f };
		o.as_sphere.radius = 1.5f;
		o.color = { .9f, .3f, .9f };
		o.reflectance = .5f;
	}
	{
		Object &o = objects[4];
		o.type = SPHERE;
		o.as_sphere.center = { -10.f, 8.f, 40.f };
		o.as_sphere.radius = 10.f;
		o.color = { .5f, .5f, .5f };
		o.reflectance = .5f;
	}

	return objects;
}

static const Object *hittest_all_objects(const std::vector<Object> &objects, const Object *const ignore, const glm::vec3 &start, const glm::vec3 &dir, glm::vec3 &hitpoint, glm::vec3 &hitnormal)
{
	float smallest = 1.f / 0.f;
	const Object *found = nullptr;
	for (size_t i = 0; i < objects.size(); ++i) {
		if (&objects[i] == ignore)
			continue;
		float dist;
		glm::vec3 point;
		glm::vec3 normal;
		if (hittest(objects[i], start, dir, dist, point, normal) && dist < smallest) {
			smallest = dist;
			hitpoint = point;
			hitnormal = normal;
			found = &objects[i];
		}
	}
	return found;
}

static glm::vec3 lighting_equation(const glm::vec3 &color, const glm::vec3 &dir, const glm::vec3 &n)
{
	constexpr float shininess = 15.f;
	const glm::vec3 light_dir LIGHT_DIR;
	const float d = std::max(glm::dot(light_dir, n), 0.f);
	const float s = std::max(glm::dot(glm::normalize(dir + light_dir), n), 0.f);
	return color * d + std::pow(s, shininess);
}

static void write_bmp(std::ofstream &file, const Pixel *const data)
{
	const byte HEADER[0x36] = { 0x42, 0x4D, 0x36, 0xEC, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x38, 0x04, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	file.write(reinterpret_cast<const char*>(HEADER), sizeof(HEADER));
	file.write(reinterpret_cast<const char*>(data), LEN * sizeof(Pixel));
}

static glm::vec3 get_skybox(const glm::vec3 &dir)
{
	constexpr float two_pi = 6.28318f;
	constexpr float pi = 3.14159f;
	const float u = .5f + std::atan2(dir.x, dir.z) / two_pi;
	const float v = .5f - std::asin(dir.y) / pi;
	constexpr float sx = SKYBOX_W - 2.f;
	constexpr float sy = SKYBOX_H - 2.f;
	const size_t x = static_cast<size_t>(u * sx + 0.5f);
	const size_t y = static_cast<size_t>(v * sy + 0.5f);
	return skybox[y * SKYBOX_W + x];
}

static glm::vec3 trace_at(const int bounce_num, const Object *const exclude, const glm::vec3 &start, const glm::vec3 &dir, const std::vector<Object> &objects)
{
	constexpr int MAX_BOUNCE = 5;
	if (bounce_num >= MAX_BOUNCE) {
		return { 1.f, 1.f, 1.f };
	}
	glm::vec3 hitpoint;
	glm::vec3 hitnormal;
	const Object *const found = hittest_all_objects(objects, exclude, start, dir, hitpoint, hitnormal);
	if (found == nullptr) {
		return get_skybox(dir);
	}
	{
		glm::vec3 dummy[2];
		const glm::vec3 light_dir LIGHT_DIR;
		const Object *const in_shadow = hittest_all_objects(objects, found, hitpoint, -light_dir, dummy[0], dummy[1]);
		if (in_shadow) {
			return found->color * SHADOW_DARKNESS;
		}
	}
	const glm::vec3 bounce_color = trace_at(bounce_num + 1, found, hitpoint, glm::reflect(dir, hitnormal), objects);
	/*
	glm::vec3 bounce_color { 0.f, 0.f, 0.f };
	for (int i = 0; i < NUM_RECAST; ++i) {
		const glm::vec3 orth = random_orthogonal(hitnormal);
		const glm::vec3 new_dir = -glm::normalize(orth * randf + hitnormal);
		const glm::vec3 c = trace_at(bounce_num + 1, hitpoint, new_dir, objects);
		bounce_color += c / static_cast<float>(NUM_RECAST);
	}
	*/
	const float b = found->reflectance;
	const float a = 1.f - b;
	const glm::vec3 my_color = lighting_equation(found->color, dir, hitnormal);
	return glm::clamp(my_color * a + bounce_color * b, 0.f, 1.f);
}
static bool load_skybox()
{
	std::ifstream file("skybox.raw", std::ios::binary);
	if (! file.good())
		return false;
	constexpr size_t len = SKYBOX_W * SKYBOX_H;
	auto pixels = std::make_unique<int[]>(len);
	file.read(reinterpret_cast<char*>(pixels.get()), sizeof(int) * len);
	for (size_t i = 0; i < len; ++i) {
		const byte b = static_cast<byte>((pixels[i] >> 24) & 0xff);
		const byte g = static_cast<byte>((pixels[i] >> 16) & 0xff);
		const byte r = static_cast<byte>((pixels[i] >> 8) & 0xff);
		skybox[i] = {r / 255.f, g / 255.f, b / 255.f };
	}
	return true;
}

int main()
{
	constexpr const char *filename = "out.bmp";
	constexpr float FOCAL_LENGTH = 2.f;
	if (! load_skybox()) {
		std::cout << "skybox texture cannot be loaded" << std::endl;
		return 1;
	}
	std::ofstream file(filename, std::ios::binary);
	if (! file.good()) {
		std::cout << "file '" << filename << "' cannot be opened for writing" << std::endl;
		return 1;
	}
	std::vector<Object> objects = generate_objects();
	auto buf = std::make_unique<Pixel[]>(LEN);
	const glm::vec3 eye_pos { 0.f, 0.f, -FOCAL_LENGTH };
	for (int i = 0; i < LEN; ++i) {
		constexpr float scale = 2.f / static_cast<float>(H);
		const int pixel_x = i % W;
		const int pixel_y = i / W;
		const glm::vec3 p { scale * pixel_x - 1.7778f, -scale * pixel_y + 1.f, 0.f };
		const glm::vec3 dir = glm::normalize(p - eye_pos);
		buf[i].from_color(trace_at(0, nullptr, p, glm::normalize(dir), objects));
	}
	write_bmp(file, buf.get());
	return 0;
}
