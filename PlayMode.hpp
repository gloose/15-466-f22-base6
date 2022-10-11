#include "Mode.hpp"

#include "Scene.hpp"

#include "Connection.hpp"
#include "Game.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

//#include "../nest-libs/windows/glm/include/glm/glm.hpp"
//#include "../nest-libs/windows/harfbuzz/include/hb.h"
//#include "../nest-libs/windows/harfbuzz/include/hb-ft.h"
//#include "../nest-libs/windows/freetype/include/freetype/freetype.h"
//#include "../nest-libs/windows/freetype/include/freetype/fttypes.h"
#include <glm/glm.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	// Properties of the font used in the game
	// (Borrowed from Game 4)
	FT_Library ft_library;
	FT_Face ft_face;
	hb_font_t* hb_font;
	uint32_t char_top = 1;
	uint32_t char_bottom = 1;
	uint32_t char_width = 1;
	uint32_t char_height = 1;
	int font_size = 100;

	static inline glm::u8vec4 default_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);

	// Structures borrowed from Game 4 for text rendering

	//In order to implement the PPU466 on modern graphics hardware, a fancy, special purpose tile-drawing shader is used:
	struct PPUTileProgram {
		PPUTileProgram();
		~PPUTileProgram();

		GLuint program = 0;

		//Attribute (per-vertex variable) locations:
		GLuint Position_vec2 = -1U;
		GLuint TileCoord_ivec2 = -1U;
		GLuint Color_vec4 = -1U;

		//Uniform (per-invocation variable) locations:
		GLuint OBJECT_TO_CLIP_mat4 = -1U;

		//Textures bindings:
		//TEXTURE0 - the tile table (as a 128x128 R8UI texture)
	};

	//PPU data is streamed to the GPU (read: uploaded 'just in time') using a few buffers:
	struct PPUDataStream {
		PPUDataStream();
		~PPUDataStream();

		//vertex format for convenience:
		struct Vertex {
			Vertex(glm::ivec2 const& Position_, glm::ivec2 const& TileCoord_, glm::u8vec4 Color_)
				: Position(Position_), TileCoord(TileCoord_), Color(glm::vec4(Color_) / 255.f) { }
			//I generally make class members lowercase, but I make an exception here because
			// I use uppercase for vertex attributes in shader programs and want to match.
			glm::ivec2 Position;
			glm::ivec2 TileCoord;
			glm::vec4 Color;
		};

		//vertex buffer that will store data stream:
		GLuint vertex_buffer = 0;

		//vertex array object that maps tile program attributes to vertex storage:
		GLuint vertex_buffer_for_tile_program = 0;

		//texture object that will store tile table:
		GLuint tile_tex = 0;
	};

	// Adapted from PPU466
	glm::u8vec3 background_color = glm::u8vec3(0x00, 0x00, 0x00);
	enum : uint32_t {
		ScreenWidth = 720,
		ScreenHeight = 720
	};

	std::vector<uint32_t> characters = {
		0x20,    // 0: Space
		0x270A,  // 1: Rock
		0x270B,  // 2: Paper
		0x270C,  // 3: Scissors
		0x1F44D, // 4: Thumbs up
		0x1F44E, // 5: Thumbs down
		0x261D   // 6: One finger
	};

	enum character_code {
		Char_Space,
		Char_Rock,
		Char_Paper,
		Char_Scissors,
		Char_Thumbs_Up,
		Char_Thumbs_Down,
		Char_One
	};

	glm::u8vec4 win_color = glm::u8vec4(0x00, 0xff, 0x00, 0xff);
	glm::u8vec4 lose_color = glm::u8vec4(0xff, 0x00, 0x00, 0xff);

	// Helper functions
	int drawText(std::vector<uint32_t> text, glm::vec2 position, size_t width, std::vector<PPUDataStream::Vertex>* triangle_strip, glm::u8vec4 color = default_color);
	void drawTriangleStrip(const std::vector<PPUDataStream::Vertex>& triangle_strip);
	void drawCharacter(glm::vec2 pos, uint32_t tile_index, glm::u8vec4 tile_color, std::vector<PPUDataStream::Vertex>* triangle_strip);
};
