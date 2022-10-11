#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"
#include "gl_compile_program.hpp"
#include "read_write_chunk.hpp"
#include "Load.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>
#include <fstream>
#include <filesystem>

//#include "../nest-libs/windows/glm/include/glm/gtc/type_ptr.hpp"
//#include "../nest-libs/windows/harfbuzz/include/hb.h"
//#include "../nest-libs/windows/harfbuzz/include/hb-ft.h"
//#include "../nest-libs/windows/freetype/include/freetype/freetype.h"
//#include "../nest-libs/windows/freetype/include/freetype/fttypes.h"
#include <glm/gtc/type_ptr.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>

Load< PlayMode::PPUTileProgram > tile_program(LoadTagEarly); //will 'new PPUTileProgram()' by default
Load< PlayMode::PPUDataStream > data_stream(LoadTagDefault);

PlayMode::PlayMode(Client &client_) : client(client_) {
	// Adapted from Harfbuzz example linked on assignment page
	// This font was obtained from https://fonts.google.com/noto/specimen/Noto+Emoji
	// See the license in dist/Noto_Emoji/OFL.txt
	std::string fontfilestring = data_path("Noto_Emoji/static/NotoEmoji-Regular.ttf");
	const char* fontfile = fontfilestring.c_str();

	// Initialize FreeType and create FreeType font face.
	if (FT_Init_FreeType(&ft_library))
		abort();
	if (FT_New_Face(ft_library, fontfile, 0, &ft_face))
		abort();
	if (FT_Set_Char_Size(ft_face, font_size * 64, font_size * 64, 0, 0))
		abort();

	// Create hb-ft font.
	hb_font = hb_ft_font_create(ft_face, NULL);

	// Determine a fixed size and baseline for all character tiles
	for (size_t i = 1; i <= characters.size(); i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(ft_face, (FT_ULong)characters[i]);
		FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);

		uint32_t w = ft_face->glyph->bitmap.width + ft_face->glyph->bitmap_left;

		int top = ft_face->glyph->bitmap_top;
		int bottom = ft_face->glyph->bitmap.rows - top;

		if (w > char_width) {
			char_width = w;
		}
		if (top > (int)char_top) {
			char_top = top;
		}
		if (bottom > (int)char_bottom) {
			char_bottom = bottom;
		}
	}
	char_height = char_top + char_bottom;

	//interpret tiles and build a 1 x num_chars color texture (adapated from PPU466)
	std::vector<glm::u8vec4> data;
	data.resize(characters.size() * char_width * char_height);
	for (uint32_t i = 0; i < characters.size(); i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(ft_face, characters[i]);//min_char + i);
		FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);

		//location of tile in the texture:
		uint32_t ox = i * char_width;

		//copy tile indices into texture:
		for (int y = 0; y < (int)char_height; y++) {
			for (int x = 0; x < (int)char_width; x++) {
				int bitmap_x = x - ft_face->glyph->bitmap_left;
				int bitmap_baseline = ft_face->glyph->bitmap_top;
				int from_baseline = y - char_bottom;
				int bitmap_y = bitmap_baseline - from_baseline;

				if (bitmap_x >= 0 && bitmap_x < (int)ft_face->glyph->bitmap.width && bitmap_y >= 0 && bitmap_y < (int)ft_face->glyph->bitmap.rows) {
					data[ox + x + (char_width * characters.size()) * y] = glm::u8vec4(0xff, 0xff, 0xff, ft_face->glyph->bitmap.buffer[bitmap_x + ft_face->glyph->bitmap.width * bitmap_y]);
				}
				else {
					data[ox + x + (char_width * characters.size()) * y] = glm::u8vec4(0, 0, 0, 0);
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, data_stream->tile_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, char_width * (int)characters.size(), char_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glBindTexture(GL_TEXTURE_2D, 0);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	auto pressFinger = [&](std::array<Button, 4>& hand, int finger) {
		hand[finger].downs += 1;
		hand[finger].pressed = true;
	};

	auto releaseFinger = [&](std::array<Button, 4>& hand, int finger) {
		hand[finger].pressed = false;
	};

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_f) {
			pressFinger(controls.left_buttons, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			pressFinger(controls.left_buttons, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			pressFinger(controls.left_buttons, 2);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			pressFinger(controls.left_buttons, 3);
			return true;
		} else if (evt.key.keysym.sym == SDLK_j) {
			pressFinger(controls.right_buttons, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			pressFinger(controls.right_buttons, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			pressFinger(controls.right_buttons, 2);
			return true;
		} else if (evt.key.keysym.sym == SDLK_SEMICOLON) {
			pressFinger(controls.right_buttons, 3);
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_f) {
			releaseFinger(controls.left_buttons, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			releaseFinger(controls.left_buttons, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			releaseFinger(controls.left_buttons, 2);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			releaseFinger(controls.left_buttons, 3);
			return true;
		} else if (evt.key.keysym.sym == SDLK_j) {
			releaseFinger(controls.right_buttons, 0);
			return true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			releaseFinger(controls.right_buttons, 1);
			return true;
		} else if (evt.key.keysym.sym == SDLK_l) {
			releaseFinger(controls.right_buttons, 2);
			return true;
		} else if (evt.key.keysym.sym == SDLK_SEMICOLON) {
			releaseFinger(controls.right_buttons, 3);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	for (size_t i = 0; i < controls.left_buttons.size(); i++) {
		controls.left_buttons[i].downs = 0;
	}
	for (size_t i = 0; i < controls.right_buttons.size(); i++) {
		controls.right_buttons[i].downs = 0;
	}

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);
}

// Draws a single character centered at pos to a triangle strip (adapted from PPU466)
void PlayMode::drawCharacter(glm::vec2 pos, uint32_t tile_index, glm::u8vec4 tile_color, std::vector<PPUDataStream::Vertex>* triangle_strip) {
	// Convert tile index to lower-left pixel coordinate in tile image:
	glm::ivec2 tile_coord = glm::ivec2(tile_index * char_width, 0);

	// Put the position into screen coordinates
	pos.x = ScreenWidth / 2.f * (1 + pos.x);
	pos.y = ScreenHeight / 2.f * (1 + pos.y);

	// Build a quad as a (very short) triangle strip that starts and ends with degenerate triangles:
	triangle_strip->emplace_back(glm::ivec2(pos.x - char_width / 2.f, pos.y - char_height / 2.f), glm::ivec2(tile_coord.x + 0, tile_coord.y + 0), tile_color);
	triangle_strip->emplace_back(triangle_strip->back());
	triangle_strip->emplace_back(glm::ivec2(pos.x - char_width / 2.f, pos.y + char_height / 2.f), glm::ivec2(tile_coord.x + 0, tile_coord.y + char_height), tile_color);
	triangle_strip->emplace_back(glm::ivec2(pos.x + char_width / 2.f, pos.y - char_height / 2.f), glm::ivec2(tile_coord.x + char_width, tile_coord.y + 0), tile_color);
	triangle_strip->emplace_back(glm::ivec2(pos.x + char_width / 2.f, pos.y + char_height / 2.f), glm::ivec2(tile_coord.x + char_width, tile_coord.y + char_height), tile_color);
	triangle_strip->emplace_back(triangle_strip->back());
}

void PlayMode::drawTriangleStrip(const std::vector<PPUDataStream::Vertex>& triangle_strip) {
	// Upload vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, data_stream->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(decltype(triangle_strip[0])) * triangle_strip.size(), triangle_strip.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set up the pipeline:
	// set blending function for output fragments:
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// set the shader programs:
	glUseProgram(tile_program->program);

	// configure attribute streams:
	glBindVertexArray(data_stream->vertex_buffer_for_tile_program);

	// set uniforms for shader programs:
	{ //set matrix to transform [0,ScreenWidth]x[0,ScreenHeight] -> [-1,1]x[-1,1]:
		//NOTE: glm uses column-major matrices:
		glm::mat4 OBJECT_TO_CLIP = glm::mat4(
			glm::vec4(2.0f / ScreenWidth, 0.0f, 0.0f, 0.0f),
			glm::vec4(0.0f, 2.0f / ScreenHeight, 0.0f, 0.0f),
			glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
			glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)
		);
		glUniformMatrix4fv(tile_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(OBJECT_TO_CLIP));
	}

	// bind texture units to proper texture objects:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, data_stream->tile_tex);

	//now that the pipeline is configured, trigger drawing of triangle strip:
	glDrawArrays(GL_TRIANGLE_STRIP, 0, GLsizei(triangle_strip.size()));

	GL_ERRORS();
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	static std::array< glm::vec2, 64 > const circle = [](){
		std::array< glm::vec2, 64 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = 1.0f;
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);
	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	DrawLines lines(world_to_clip);
	
	if (game.players.size() == 3) {
		// Helper to draw a line from a to b of width w, with normals n1 and n2 at the endpoints (to make lines join together nicely)
		auto thickLine = [&](glm::vec2 a, glm::vec2 b, int w, glm::u8vec4 c, glm::vec2 n1, glm::vec2 n2) {
			n1 = glm::normalize(n1);
			n2 = glm::normalize(n2);
			float angle = atan2((b - a).y, (b - a).x);
			float perp_angle = angle + (float)M_PI / 2.f;
			glm::vec2 perp(cos(perp_angle), sin(perp_angle));
			glm::vec2 offset(perp.x * 1.f / ScreenHeight, perp.y * 1.f / ScreenHeight);
			
			for (float i = 0; i < 100; i++) {
				glm::vec2 off_a = n1 / glm::dot(perp, n1) * (float)i * (1.f / ScreenHeight);
				glm::vec2 off_b = n2 / glm::dot(perp, n2) * (float)i * (1.f / ScreenHeight);
				if ((int)round(abs(glm::dot(off_a, perp)) * ScreenHeight) > w || (int)round(abs(glm::dot(off_b, perp)) * ScreenHeight) > w) {
					break;
				}
				lines.draw(glm::vec3(a + off_a, 0.f), glm::vec3(b + off_b, 0.f), c);
				lines.draw(glm::vec3(a - off_a, 0.f), glm::vec3(b - off_b, 0.f), c);
			}
		};

		// Find various properties of all players and put them into arrays by player index
		std::vector<glm::vec2> positions;
		std::vector<float> angles;
		std::vector<glm::u8vec4> colors;
		positions.resize(game.players.size());
		angles.resize(game.players.size());
		colors.resize(game.players.size());
		for (auto const& player : game.players) {
			//positions[player.index] = player.position;
			if (game.players.size() == 1) {
				positions[player.index] = glm::vec2(0, 0);
			} else {
				float base_angle = 3 * (float)M_PI / 2;
				for (auto& p : game.players) {
					int8_t relative_index = (p.index + (int8_t)game.players.size() - game.players.front().index) % game.players.size();
					angles[p.index] = base_angle + relative_index * 2 * (float)M_PI / game.players.size();
					positions[p.index] = glm::vec2(cosf(angles[p.index]) * game.triangle_radius, sinf(angles[p.index]) * game.triangle_radius);
					colors[p.index] = p.color;
				}
			}
		}

		// Set background color to blend into that of the winning player
		glm::vec4 clear_color = game.bary_score[0] * (glm::vec4)colors[0] + game.bary_score[1] * (glm::vec4)colors[1] + game.bary_score[2] * (glm::vec4)colors[2];
		clear_color = clear_color / (float)(2 * 0xFF);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Draw triangle
		for (size_t i = 0; i < positions.size(); i++) {
			size_t next_i = (i + 1) % positions.size();
			thickLine(glm::vec3(positions[i], 0.f), glm::vec3(positions[next_i], 0.f), 8, glm::u8vec4(0xff, 0xff, 0xff, 0xff), positions[i], positions[next_i]);
		}
		
		for (size_t i = 0; i < positions.size(); i++) {
			size_t next_i = (i + 1) % positions.size();
			glm::vec2 midpoint = (positions[i] + positions[next_i]) / 2.f;
			thickLine(glm::vec3(positions[i], 0.f), glm::vec3(midpoint, 0.f), 4, colors[i], positions[i], midpoint);
			thickLine(glm::vec3(positions[next_i], 0.f), glm::vec3(midpoint, 0.f), 4, colors[next_i], positions[next_i], midpoint);
		}
		

		
		float player_radius = glm::length(positions[game.players.front().index] - positions[game.players.back().index]) / 2.f;
		std::vector< PPUDataStream::Vertex > triangle_strip;
		for (auto const &player : game.players) {
			// Draw stamina circle
			for (uint32_t a = 0; a < circle.size(); ++a) {
				thickLine(
					glm::vec3(positions[player.index] + player_radius * abs(player.stamina) / player.max_stamina * circle[a], 0.0f),
					glm::vec3(positions[player.index] + player_radius * abs(player.stamina) / player.max_stamina * circle[(a+1)%circle.size()], 0.0f),
					8,
					player.stamina > 0 ? glm::u8vec4(0xff, 0xff, 0xff, 0xff) : glm::u8vec4(0x00, 0x00, 0x00, 0x00),
					circle[a],
					circle[(a+1)%circle.size()]
				);
			}

			// Draw player boundary circle
			std::array<int, 2> widths = {
				8,
				4
			};
			std::array<glm::u8vec4, 2> ring_colors = {
				glm::u8vec4(0xff, 0xff, 0xff, 0xff),
				player.color
			};
			for (int i = 0; i < 2; i++) {
				for (uint32_t a = 0; a < circle.size(); ++a) {
					thickLine(
						glm::vec3(positions[player.index] + player_radius * circle[a], 0.0f),
						glm::vec3(positions[player.index] + player_radius * circle[(a+1)%circle.size()], 0.0f),
						widths[i],
						ring_colors[i],
						circle[a],
						circle[(a+1)%circle.size()]
					);
				}
			}

			// Get pointers to the players on either side of this one
			Player* left_player = nullptr;
			Player* right_player = nullptr;
			for (auto& p2 : game.players) {
				if (p2.index == (int8_t)((game.players.size() + player.index - 1) % game.players.size())) {
					left_player = &p2;
				}
				if (p2.index == (int8_t)((game.players.size() + player.index + 1) % game.players.size())) {
					right_player = &p2;
				}
			}
			assert(left_player != nullptr && right_player != nullptr && "Missing player");

			// Determine where to draw the hands
			glm::vec2 left_hand_pos = positions[player.index] + (positions[left_player->index] - positions[player.index]) * 0.4f;
			glm::vec2 right_hand_pos = positions[player.index] + (positions[right_player->index] - positions[player.index]) * 0.4f;
			left_hand_pos *= 1.5f;
			right_hand_pos *= 1.5f;
			
			if (!game.over) {
				// Color the active player's hands based on whether they are winning or losing against the neighbors
				glm::u8vec4 left_color = default_color;
				glm::u8vec4 right_color = default_color;
				if (&player == &game.players.front() && game.players.size() == 3) {
					auto winning = [](Hand h1, Hand h2) -> bool {
						return h2 == Hand::None ||
							(h1 == Hand::Rock && h2 == Hand::Scissors) ||
							(h1 == Hand::Paper && h2 == Hand::Rock) ||
							(h1 == Hand::Scissors && h2 == Hand::Paper);
					};

					if (winning(player.left_hand, left_player->right_hand)) {
						left_color = win_color;
					} else if (winning(left_player->right_hand, player.left_hand)) {
						left_color = lose_color;
					}

					if (winning(player.right_hand, right_player->left_hand)) {
						right_color = win_color;
					} else if (winning(right_player->left_hand, player.right_hand)) {
						right_color = lose_color;
					}
				}

				// Helper to get the character index associated with a hand
				auto getCharacter = [](Hand hand) -> uint32_t {
					switch (hand) {
						case Rock:
							return Char_Rock;
						case Paper:
							return Char_Paper;
						case Scissors:
							return Char_Scissors;
						case None:
							return Char_Space;
					}
					return Char_Space;
				};

				// Draw the hands
				if (getCharacter(player.left_hand) && player.stamina > 0) {
					drawCharacter(left_hand_pos, getCharacter(player.left_hand), left_color, &triangle_strip);
				}
				if (getCharacter(player.right_hand) && player.stamina > 0) {
					drawCharacter(right_hand_pos, getCharacter(player.right_hand), right_color, &triangle_strip);
				}
			} else {
				// Draw thumbs up/down depending on whether the player won or lost
				drawCharacter(left_hand_pos, player.win ? Char_Thumbs_Up : Char_Thumbs_Down, default_color, &triangle_strip);
				drawCharacter(right_hand_pos, player.win ? Char_Thumbs_Up : Char_Thumbs_Down, default_color, &triangle_strip);
			}
		}
		drawTriangleStrip(triangle_strip);

		{ // Draw score point
			glm::vec2 score_pos(0.f, 0.f);
			float score_radius = 0.02f;
			for (auto const& player : game.players) {
				score_pos += game.bary_score[player.index] * positions[player.index];
			}

			std::array<glm::u8vec4, 2> ring_colors = {
				glm::u8vec4(0xFF, 0xFF, 0xFF, 0xFF),
				glm::u8vec4(0x00, 0x00, 0x00, 0xFF)
			};
			std::array<int, 2> widths = {6, 2};

			for (int i = 0; i < 2; i++) {
				for (uint32_t a = 0; a < circle.size(); ++a) {
					thickLine(
						glm::vec3(score_pos + score_radius * circle[a], 0.0f),
						glm::vec3(score_pos + score_radius * circle[(a+1)%circle.size()], 0.0f),
						widths[i],
						ring_colors[i],
						circle[a],
						circle[(a+1)%circle.size()]
					);
				}
			}
		}
	} else {
		// Draw a hand holding up fingers equal to the number of players joined
		std::vector< PPUDataStream::Vertex > triangle_strip;
		drawCharacter(glm::vec2(0, 0), game.players.size() == 1 ? Char_One : Char_Scissors, default_color, &triangle_strip);
		drawTriangleStrip(triangle_strip);
	}
	
	GL_ERRORS();
}








// The following is borroweed from Game 4
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

PlayMode::PPUTileProgram::PPUTileProgram() {
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in ivec2 TileCoord;\n"
		"out vec2 tileCoord;\n"
		"in vec4 Color;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	tileCoord = TileCoord;\n"
		"	color = Color;\n"
		"}\n"
		,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TILE_TABLE;\n"
		"in vec2 tileCoord;\n"
		"out vec4 fragColor;\n"
		"in vec4 color;\n"
		"void main() {\n"
		"fragColor = texelFetch(TILE_TABLE, ivec2(tileCoord), 0);\n"
		"fragColor.r = color.r;\n"
		"fragColor.g = color.g;\n"
		"fragColor.b = color.b;\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	Position_vec2 = glGetAttribLocation(program, "Position");
	TileCoord_ivec2 = glGetAttribLocation(program, "TileCoord");
	Color_vec4 = glGetAttribLocation(program, "Color");
	//Palette_int = glGetAttribLocation(program, "Palette");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");

	GLuint TILE_TABLE_usampler2D = glGetUniformLocation(program, "TILE_TABLE");
	//GLuint PALETTE_TABLE_sampler2D = glGetUniformLocation(program, "PALETTE_TABLE");

	//bind texture units indices to samplers:
	glUseProgram(program);
	glUniform1i(TILE_TABLE_usampler2D, 0);
	//glUniform1i(PALETTE_TABLE_sampler2D, 1);
	glUseProgram(0);

	GL_ERRORS();
}

PlayMode::PPUTileProgram::~PPUTileProgram() {
	if (program != 0) {
		glDeleteProgram(program);
		program = 0;
	}
}

//PPU data is streamed to the GPU (read: uploaded 'just in time') using a few buffers:
PlayMode::PPUDataStream::PPUDataStream() {

	//vertex_buffer_for_tile_program is a vertex array object that tells the GPU the layout of data in vertex_buffer:
	glGenVertexArrays(1, &vertex_buffer_for_tile_program);
	glBindVertexArray(vertex_buffer_for_tile_program);

	//vertex_buffer will (eventually) hold vertex data for drawing:
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	//Notice how this binding is attaching an integer input to a floating point attribute:
	glVertexAttribPointer(
		tile_program->Position_vec2, //attribute
		2, //size
		GL_INT, //type
		GL_FALSE, //normalized
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, Position) //offset
	);
	glEnableVertexAttribArray(tile_program->Position_vec2);

	//the "I" variant binds to an integer attribute:
	glVertexAttribIPointer(
		tile_program->TileCoord_ivec2, //attribute
		2, //size
		GL_INT, //type
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, TileCoord) //offset
	);
	glEnableVertexAttribArray(tile_program->TileCoord_ivec2);

	// Add color attribute
	glVertexAttribPointer(
		tile_program->Color_vec4, //attribute
		4, //size
		GL_FLOAT, //type
		GL_FALSE, //normalized
		sizeof(Vertex), //stride
		(GLbyte*)0 + offsetof(Vertex, Color) //offset
	);
	glEnableVertexAttribArray(tile_program->Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glGenTextures(1, &tile_tex);
	glBindTexture(GL_TEXTURE_2D, tile_tex);
	//passing 'nullptr' to TexImage says "allocate memory but don't store anything there":
	// (textures will be uploaded later)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 26, 26, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	//make the texture have sharp pixels when magnified:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//when access past the edge, clamp to the edge:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	GL_ERRORS();
}

PlayMode::PPUDataStream::~PPUDataStream() {
	if (vertex_buffer_for_tile_program != 0) {
		glDeleteVertexArrays(1, &vertex_buffer_for_tile_program);
		vertex_buffer_for_tile_program = 0;
	}
	if (vertex_buffer != 0) {
		glDeleteBuffers(1, &vertex_buffer);
		vertex_buffer = 0;
	}
	if (tile_tex != 0) {
		glDeleteTextures(1, &tile_tex);
		tile_tex = 0;
	}
}
