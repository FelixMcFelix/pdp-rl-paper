# C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\bump\bump.mak
# This make file is for project bump
# 08/13/20 12:17:41
# Programmer Studio 6.1.0.1-preview.3286
# Copyright (C) 2008-2018 Netronome Systems, Inc.  All rights reserved.

./Makefile-nfp4build: FORCE

FORCE:
	nfp4build --output-nffw-filename ./out/nfp_6x_40.nffw \
		--incl-p4-build ./wirebump.p4 \
		--sku AMDA0058-0012:0 \
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
		--nfirc_no_mac_ingress_timestamp
