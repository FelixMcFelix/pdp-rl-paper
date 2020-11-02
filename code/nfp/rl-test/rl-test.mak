# C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\rl-test.mak
# This make file is for project rl-test
# 11/02/20 17:12:12
# Programmer Studio 6.1.0.1-preview.3286
# Copyright (C) 2008-2018 Netronome Systems, Inc.  All rights reserved.

./Makefile-nfp4build: FORCE

FORCE:
	nfp4build --output-nffw-filename ./out/rl-test.nffw \
		--sandbox-c ./rl-test.c \
		--sandbox-c ./rl-pkt-store.c \
		--incl-p4-build ./rl-test.p4 \
		--sku AMDA0078-0012:0 \
		--platform starfighter1 \
		--reduced-thread-usage \
		--no-shared-codestore \
		--debug-info \
		--nfp4c_p4_version 16 \
		--nfp4c_p4_compiler p4c-nfp \
		--nfirc_default_table_size 65536 \
		--nfirc_no_all_header_ops \
		--nfirc_implicit_header_valid \
		--nfirc_no_zero_new_headers \
		--nfirc_multicast_group_count 16 \
		--nfirc_multicast_group_size 16 \
		--nfirc_mac_ingress_timestamp
