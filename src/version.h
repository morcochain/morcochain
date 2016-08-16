// Copyright (c) 2013-2014 The Peershares developers
// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2013-2014 The Peershares developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NU_VERSION_H
#define NU_VERSION_H

#include "clientversion.h"
#include <string>

//
// client versioning
//

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_DATE;

//
// network protocol versioning
//

// V0.4.5
static const int PROTOCOL_V0_4_5 = 40500;

// V0.5
static const int PROTOCOL_V0_5 = 50000;
static const unsigned int PROTOCOL_V5_SWITCH_TIME      = 1415368800; // 2014-11-07 14:00:00 UTC
static const unsigned int PROTOCOL_V5_TEST_SWITCH_TIME = 1414195200; // 2014-10-25 00:00:00 UTC

// V2.0
static const int PROTOCOL_V2_0 = 2000000;
static const unsigned int PROTOCOL_V2_0_VOTE_TIME      = 1440511200; // 2015-08-25 14:00:00 UTC
static const unsigned int PROTOCOL_V2_0_TEST_VOTE_TIME = 1433858400; // 2015-06-09 14:00:00 UTC

#ifdef TESTING
static const unsigned int PROTOCOL_SWITCH_REQUIRE_VOTES = 9;
static const unsigned int PROTOCOL_SWITCH_COUNT_VOTES = 10;
#else
static const unsigned int PROTOCOL_SWITCH_REQUIRE_VOTES = 1800;
static const unsigned int PROTOCOL_SWITCH_COUNT_VOTES = 2000;
#endif

// Current client protocol
static const int PROTOCOL_VERSION = PROTOCOL_V2_0;

// earlier versions not supported as of Feb 2012, and are disconnected
// NOTE: as of bitcoin v0.6 message serialization (vSend, vRecv) still
// uses MIN_PROTO_VERSION(209), where message format uses PROTOCOL_VERSION
static const int MIN_PROTO_VERSION = 3;

// nTime field added to CAddress, starting with this version;
// if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 1;

// only request blocks from nodes outside this range of versions
static const int NOBLKS_VERSION_START = 1;
static const int NOBLKS_VERSION_END = 1;

// BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 1;

// "mempool" command, enhanced "getdata" behavior starts with this version:
static const int MEMPOOL_GD_VERSION = 60002;

#endif
