#ifndef _RL_PROTO_H
#define _RL_PROTO_H

enum rl_packet_type {
	RL_CONFIG, // -> rl_pkt_config
	RL_INSTALL, // -> rl_pkt_install
};

struct rl_packet {
	enum rl_packet_type type;
	// decode next struct
};

enum rl_packet_config_type {
	RL_CFG_TILE,
};

struct rl_pkt_config {
	enum rl_pkt_config_type type;
};

struct rl_pkt_install {
	uint32_t offset;
	uint8_t d_type;
};


#endif /* !_RL_PROTO_H_ */
