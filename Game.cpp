#define _USE_MATH_DEFINES

#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 8;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	for (size_t i = 0; i < left_buttons.size(); i++) {
		send_button(left_buttons[i]);
	}
	for (size_t i = 0; i < right_buttons.size(); i++) {
		send_button(right_buttons[i]);
	}
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 8) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 8!");
	
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	for (size_t i = 0; i < left_buttons.size(); i++) {
		recv_button(recv_buffer[4 + i], &left_buttons[i]);
	}
	for (size_t i = 0; i < right_buttons.size(); i++) {
		recv_button(recv_buffer[4 + left_buttons.size() + i], &right_buttons[i]);
	}

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
}

Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	player.index = (uint8_t)players.size() - 1;

	std::array<glm::u8vec4, 3> colors = {
		glm::u8vec4(0xff, 0x00, 0x88, 0xff),
		glm::u8vec4(0x00, 0xff, 0xee, 0xff),
		glm::u8vec4(0xff, 0xbb, 0x00, 0xff)
	};
	player.color = colors[player.index];

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {
	// Set hand state for all players
	for (auto& p : players) {
		// Helper to get the hand position for a given array of button presses
		auto getHand = [&](std::array<Button, 4> buttons) -> Hand {
			if (p.stamina <= 0) {
				return Hand::None;
			}
			if (buttons[0].pressed && buttons[1].pressed && buttons[2].pressed && buttons[3].pressed) {
				return Hand::Rock;
			}
			if (!buttons[0].pressed && !buttons[1].pressed && !buttons[2].pressed && !buttons[3].pressed) {
				return Hand::Paper;
			}
			if (!buttons[0].pressed && !buttons[1].pressed && buttons[2].pressed && buttons[3].pressed) {
				return Hand::Scissors;
			}
			return Hand::None;
		};

		// Set player hands
		p.left_hand = getHand(p.controls.left_buttons);
		p.right_hand = getHand(p.controls.right_buttons);
	}

	// Perform normal game updates only if there are three players and the game is not over
	if (players.size() == 3 && !over) {
		for (auto& p : players) {
			// Helper to determine whether hand 1 beats hand 2
			auto winning = [](Hand h1, Hand h2) -> bool {
				return h2 == Hand::None ||
				      (h1 == Hand::Rock && h2 == Hand::Scissors) ||
				      (h1 == Hand::Paper && h2 == Hand::Rock) ||
				      (h1 == Hand::Scissors && h2 == Hand::Paper);
			};

			// Get pointers to the players on either side of this one
			Player* left_player = nullptr;
			Player* right_player = nullptr;
			for (auto& p2 : players) {
				if (p2.index == (players.size() + p.index - 1) % players.size()) {
					left_player = &p2;
				}
				if (p2.index == (players.size() + p.index + 1) % players.size()) {
					right_player = &p2;
				}
			}
			assert(left_player != nullptr && right_player != nullptr && "Missing player");

			// For each player that I'm beating, increase my barycentric score and decrease theirs
			if (winning(p.left_hand, left_player->right_hand)) {
				bary_score[p.index] += elapsed * score_point_speed;
				bary_score[left_player->index] -= elapsed * score_point_speed;
			}
			if (winning(p.right_hand, right_player->left_hand)) {
				bary_score[p.index] += elapsed * score_point_speed;
				bary_score[right_player->index] -= elapsed * score_point_speed;
			}

			// Expend stamina based on key presses
			if (p.stamina > 0) {
				for (size_t i = 0; i < p.controls.left_buttons.size(); i++) {
					p.stamina -= p.controls.left_buttons[i].downs;
				}
				for (size_t i = 0; i < p.controls.right_buttons.size(); i++) {
					p.stamina -= p.controls.right_buttons[i].downs;
				}
			}

			// Recover stamina
			p.stamina += p.stamina_recovery * elapsed;
			p.stamina = std::min(p.stamina, p.max_stamina);
		}

		// Check if the score point has left the triangle and the game is over
		if (bary_score.x < 0 || bary_score.y < 0 || bary_score.z < 0) {
			// Find the winning player
			float max_bary = 0;
			int8_t win_index = 0;
			for (int8_t i = 0; i < 3; i++) {
				if (bary_score[i] > max_bary) {
					max_bary = bary_score[i];
					win_index = i;
				}
			}
			
			// Set that player's "win" to true
			for (auto& p : players) {
				p.win = p.index == win_index;
			}

			// Game is over
			over = true;
		}
	} else if (over) {
		// After a certain time has elapsed, restart the game
		restart_timer += elapsed;
		if (restart_timer > restart_duration) {
			restart_timer = 0;
			over = false;
			bary_score = glm::vec3(1.f / 3.f, 1.f / 3.f, 1.f / 3.f);
			for (auto& p : players) {
				p.stamina = p.max_stamina;
			}
		}
	}
	
	// Reset 'downs' since controls have been handled:
	for (auto& p : players) {
		
		for (size_t i = 0; i < p.controls.left_buttons.size(); i++) {
			p.controls.left_buttons[i].downs = 0;
		}
		for (size_t i = 0; i < p.controls.right_buttons.size(); i++) {
			p.controls.right_buttons[i].downs = 0;
		}
	}
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	connection.send(bary_score);
	connection.send(over);

	//send player info helper:
	auto send_player = [&](Player const &player) {
		//connection.send(player.position);
		connection.send(player.color);
		connection.send(player.left_hand);
		connection.send(player.right_hand);
		connection.send(player.index);
		connection.send(player.stamina);
		connection.send(player.win);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	read(&bary_score);
	read(&over);

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.color);
		read(&player.left_hand);
		read(&player.right_hand);
		read(&player.index);
		read(&player.stamina);
		read(&player.win);
	}

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
