#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>
#include <array>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

enum Hand {
	None,
	Rock,
	Paper,
	Scissors
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		//Button left, right, up, down, jump;
		std::array<Button, 4> left_buttons;
		std::array<Button, 4> right_buttons;
		//Button l1, l2, l3, l4, l5, r1, r2, r3, r4, r5;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	glm::u8vec4 color = glm::u8vec4(0x00, 0x00, 0x00, 0x00);
	Hand left_hand = Hand::Paper;
	Hand right_hand = Hand::Paper;
	int8_t index;
	const float max_stamina = 16;
	const float stamina_recovery = 4;
	float stamina = max_stamina;
	bool win = false;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players

	glm::vec3 bary_score = glm::vec3(1.f / 3.f, 1.f / 3.f, 1.f / 3.f);
	float score_point_speed = 0.3f;

	bool over = false;
	float restart_timer = 0;
	float restart_duration = 3;

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//arena size:
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-1.f, -1.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 1.f,  1.0f);

	//player constants:
	inline static constexpr float PlayerRadius = 0.06f;
	inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;

	inline static constexpr float triangle_radius = 0.8f;
	

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
