/******************************************************************************
    Copyright (C) 2002-2022 Heroes of Argentum Developers

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "stdafx.h"

#include <iostream>
#include <signal.h>

#include "Sockets.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif


#define HOA_MAX_INPUT_BUFFER_SIZE 75000


void HoABeginCloseSocket(hoa::Socket* sctx);
void HoARealCloseSocket(hoa::Socket* sctx);

class HoASocketEvents : public hoa::SocketEvents {
public:
	virtual void onSocketNew(hoa::Socket* s) override;
	virtual void onSocketRead(hoa::Socket* s, const char* data, size_t data_len) override;
	virtual void onSocketWrite(hoa::Socket* s) override;
	virtual void onSocketClose(hoa::Socket* s) override;
};

class EchoServerSocketEvents : public hoa::SocketEvents {
public:
	virtual void onSocketNew(hoa::Socket* s) override;
	virtual void onSocketRead(hoa::Socket* s, const char* data, size_t data_len) override;
	virtual void onSocketWrite(hoa::Socket* s) override;
	virtual void onSocketClose(hoa::Socket* s) override;
};

void EchoServerSocketEvents::onSocketNew(hoa::Socket* s) {
	std::cout << "echo onSocketNew " << s->getIP() << std::endl;
}

void EchoServerSocketEvents::onSocketRead(hoa::Socket* s, const char* data, size_t data_len) {
	std::cout << "echo onSocketRead " << s->getIP() << " len: " << data_len << std::endl;
	s->write(data, data_len);
}

void EchoServerSocketEvents::onSocketWrite(hoa::Socket* s) {
	std::cout << "echo onSocketWrite " << s->getIP() << std::endl;
}

void EchoServerSocketEvents::onSocketClose(hoa::Socket* s) {
	std::cout << "echo onSocketClose " << s->getIP() << std::endl;
}


void HoASocketEvents::onSocketRead(hoa::Socket* s, const char* data, size_t data_len) {
	int UserIndex = s->userData;
	clsByteQueue* incomingData = UserList[UserIndex].incomingData.get();
	//clsByteQueue* outgoingData = UserList[UserIndex].outgoingData.get();

	try {
		incomingData->WriteBlock(data, data_len);

		if (incomingData->length() > HOA_MAX_INPUT_BUFFER_SIZE) {
			LogApiSock("WsApiEnviar MAX_INPUT_BUFFER_SIZE UserIndex=" + vb6::CStr(UserIndex));
			HoABeginCloseSocket(s);
			return;
		}

		UserList[UserIndex].IncomingDataAvailable = true;

		if (!UserList[UserIndex].ConnIgnoreIncomingData)
		{
			HandleIncomingData(UserIndex);
		}
	} catch (std::exception& ex) {
		std::cerr << "Unhandled error on HandleIncomingData: " << ex.what() << std::endl;
		LogApiSock("Unhandled error on HandleIncomingData, " + std::string(ex.what()));
		CloseSocket(s);
	} catch (...) {
		std::cerr << "UNKNOWN error on HandleIncomingData!" << std::endl;
		LogApiSock("UNKNOWN error on HandleIncomingData!");
		CloseSocket(s);
	}
}

void HoASocketEvents::onSocketWrite(hoa::Socket* s) {
	(void)s;
}

void HoASocketEvents::onSocketNew(hoa::Socket* s) {
	try {
		int UserIndex = NextOpenUser();
		std::cout << "onSocketNew::UserIndex: " << UserIndex << std::endl;
		if (UserIndex > MaxUsers || UserIndexSocketValido(UserIndex)) {
			std::cout << "onSocketNew::Max users reached, closing connection..." << std::endl;
			s->close(true);
			return;
		}

		if (UserIndex > LastUser) {
			LastUser = UserIndex;
		}

		s->userData = UserIndex;

		std::string ip(s->getIP());

		if (BanIpBuscar(ip)) {
			std::cout << "onSocketNew::IP banned, closing connection..." << std::endl;
			s->close(true);
			return;
		}

		if (!IpSecurityAceptarNuevaConexion(ip)) {
			std::cout << "onSocketNew::Can't accept new connection, closing connection..." << std::endl;
			s->close(true);
			return;
		}

		if (IPSecuritySuperaLimiteConexiones(ip)) {
			std::cout << "onSocketNew::IP connection limit reached, closing connection..." << std::endl;
			IpRestarConexion(ip);
			s->close(true);
			return;
		}

		UserList[UserIndex].incomingData.reset(new clsByteQueue());
		UserList[UserIndex].outgoingData.reset(new clsByteQueue());
		UserList[UserIndex].sockctx = s;
		UserList[UserIndex].ConnIgnoreIncomingData = false;
		UserList[UserIndex].IncomingDataAvailable = false;
		UserList[UserIndex].ip = ip;

	} catch (std::exception& ex) {
		s->close(true);
		std::cerr << "Unhandled error on do_accept: " << ex.what() << std::endl;
		LogApiSock("Unhandled error on do_accept, " + std::string(ex.what()));
	} catch (...) {
		s->close(true);
		std::cerr << "UNKNOWN error on do_accept!" << std::endl;
		LogApiSock("UNKNOWN error on do_accept!");
	}
}

void HoASocketEvents::onSocketClose(hoa::Socket* s) {
	HoABeginCloseSocket(s);
	CerrarUserIndex(s->userData);
}


std::unique_ptr<hoa::SocketServer> HoASocketServer;

HoASocketEvents sev;
EchoServerSocketEvents echosev;

void break_signal_handler(short event, void* arg) {
	(void)event;
	(void)arg;

	std::string msg = "Control-C, terminando.";

	LogApiSock(msg);
	std::cout << msg << std::endl << std::flush;

	HoASocketServer->stop();
}

void signal_handler(short event, void* arg) {
	(void)event;
	(void)arg;

	std::string msg = "signal handler: SIGPIPE";

	LogApiSock(msg);
	std::cout << msg << std::endl << std::flush;
}

void IniciaWsApi() {
	LogApiSock("IniciaWsApi");

#ifdef WIN32
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD( 2, 2 ), &WSAData) != 0) {
		perror("WSAStartup");
		LogApiSock("error en WSAStartup");
		return;
	} else { 
		std::string msg = ("Iniciado Winsock v " + std::to_string(LOBYTE( WSAData.wVersion )) + "." + std::to_string(HIBYTE( WSAData.wVersion )));
		LogApiSock(msg);
		std::cerr << msg << std::endl;
	}
#endif

	HoASocketServer = hoa::BuildSocketServer("libevent");

#ifndef _WIN32
	HoASocketServer->addSignalHandler(SIGINT, break_signal_handler, 0);
	HoASocketServer->addSignalHandler(SIGPIPE, signal_handler, 0);
#endif

	HoASocketServer->addListener("0.0.0.0", Puerto, &sev);

#if 1
	HoASocketServer->addListener("127.0.0.1", Puerto + 1, &echosev);
#endif
}

void ServerLoop() {

	HoASocketServer->loop();

}

void LimpiaWsApi() {

	HoASocketServer.release();

}

void FlushBuffer(hoa::Socket* s) {
	int UserIndex = s->userData;

	//clsByteQueue* incomingData = UserList[UserIndex].incomingData.get();
	clsByteQueue* outgoingData = UserList[UserIndex].outgoingData.get();

	if (outgoingData->length() == 0) {
			return;
	}

#if 1
	std::vector<std::int8_t> sndData;
	outgoingData->swapAndCleanRawBuffer(sndData);
#else
	std::string sndData = outgoingData->ReadBinaryFixed(outgoingData->length());
#endif

	WsApiEnviar(s, sndData);
}

void CloseSocket(hoa::Socket* s) {
	HoABeginCloseSocket(s);
}

void WsApiEnviar(int UserIndex, const char* str, std::size_t str_len) {
	if (!UserIndexSocketValido(UserIndex)) {
		throw std::runtime_error("WsApiEnviar: ConnIDValida = False, UserIndex = " + std::to_string(UserIndex));
	}

	WsApiEnviar(UserList[UserIndex].sockctx, str, str_len);
}

void WsApiEnviar(hoa::Socket* s, const char* str, std::size_t str_len) {
	int UserIndex = s->userData;
	//clsByteQueue* incomingData = UserList[UserIndex].incomingData.get();
	clsByteQueue* outgoingData = UserList[UserIndex].outgoingData.get();

	if (outgoingData->length() > 0) {
		FlushBuffer(s);
	}

	s->write(str, str_len);

	/* FIXME */
	/*
	if (evbuffer_get_length(output) > MAX_OUTPUT_BUFFER_SIZE) {
        LogApiSock("WsApiEnviar MAX_OUTPUT_BUFFER_SIZE UserIndex=" + vb6::CStr(sctx->UserIndex()));
		std::cerr << "WsApiEnviar MAX_OUTPUT_BUFFER_SIZE" << std::endl;
		return 1;
	}
*/
}

void WSApiReiniciarSockets() {
	throw std::runtime_error("WSApiReiniciarSockets not implemented");
}

bool UserIndexSocketValido(int UserIndex) {
	return UserIndex >= 1 && UserIndex <= MaxUsers && UserList[UserIndex].sockctx != nullptr;
}

void HoABeginCloseSocket(hoa::Socket* s) {
	int UserIndex = s->userData;

    if (UserIndexSocketValido(UserIndex)) {
	    IpRestarConexion (UserList[UserIndex].ip);
		UserList[UserIndex].sockctx = nullptr;
    }

    s->close(false);
}

void HoARealCloseSocket(hoa::Socket* s) {
	s->close(false);
}

void WSApiCloseSocket(int UserIndex) {
	if (!UserIndexSocketValido(UserIndex)) {
		throw std::runtime_error("WSApiCloseSocket: ConnIDValida = False, UserIndex = " + std::to_string(UserIndex));
	}

	hoa::Socket* sctx = UserList[UserIndex].sockctx;

	HoABeginCloseSocket(sctx);
}

