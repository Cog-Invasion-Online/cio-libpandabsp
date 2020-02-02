#pragma once

#include <netDatagram.h>

INLINE NetDatagram BeginMessage( int msgtype )
{
	NetDatagram dg;
	dg.add_uint16( msgtype );
	return dg;
}

enum
{
	NETMSG_SNAPSHOT,

	NETMSG_CLIENT_HELLO,
	NETMSG_HELLO_RESP,

	NETMSG_CLIENT_HEARTBEAT,
	NETMSG_SERVER_HEARTBEAT,

	NETMSG_CMD,

	NETMSG_CLIENT_STATE,
	NETMSG_CHANGE_LEVEL,
};