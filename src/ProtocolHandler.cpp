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
#include <iostream>

#include "zmq.hpp"
#include "zmq_addon.hpp"
#include "json.hpp"

#include "stdafx.h"

#include "ProtocolHandler.h"
#include "enums.h"
#include "vb6compat.h"
#include "Crypto.h"

#include "Declares.h"
#include "InvUsuario.h"

std::vector<HoAPacketHandler> UserProtocolHandler;

void HandleIncomingData(int UserIndex) {

	int k = 0;
	int lastvalidpos = UserList[UserIndex].incomingData->getReadPos();

	UserList[UserIndex].IncomingDataAvailable = false;

	while (UserList[UserIndex].incomingData->length() > 0 && k < MAX_PACKETS_PER_ITER) {
		// int startpos = UserList[UserIndex].sockctx->incomingData->getReadPos();
		try {
			HandleIncomingDataOnePacket(UserIndex);
			++k;
			if (!UserIndexSocketValido(UserIndex)) {
				break;
			}
			lastvalidpos = UserList[UserIndex].incomingData->getReadPos();
		} catch (bytequeue_data_error& ex) {
			std::cerr << "bytequeue_data_error: " << ex.what() << std::endl;
			CloseSocket(UserIndex);
			break;
		} catch (insufficient_data_error& ex) {
			std::cerr << "error: " << ex.what() << std::endl;
			UserList[UserIndex].incomingData->commitRead(lastvalidpos);
			lastvalidpos = 0;
			break;
		}
	}

	if (UserIndexSocketValido(UserIndex)) {
		UserList[UserIndex].ConnIgnoreIncomingData = (UserList[UserIndex].incomingData->length() > 0 && k == MAX_PACKETS_PER_ITER);

		if (lastvalidpos > 0) {
			UserList[UserIndex].incomingData->commitRead(lastvalidpos);
		}

		FlushBuffer(UserIndex);
	}
}

void HandleIncomingDataOnePacket(int UserIndex) {
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/09/07 */
	/* ' */
	/* '*************************************************** */
	int packetID;

	packetID = UserList[UserIndex].incomingData->PeekByte();

	/* 'Does the packet requires a logged user?? */
	if (!(packetID == hoa::protocol::client::ClientPacketID_ThrowDices || packetID == hoa::protocol::client::ClientPacketID_LoginExistingChar)) {

		/* 'Is the user actually logged? */
		if (!UserList[UserIndex].flags.UserLogged) {
			CloseSocket(UserIndex);
			return;

			/* 'He is logged. Reset idle counter if id is valid. */
		} else if (packetID <= hoa::protocol::client::ClientPacketID_PACKET_COUNT) {
			UserList[UserIndex].Counters.IdleCount = 0;
		}
	} else if (packetID <= hoa::protocol::client::ClientPacketID_PACKET_COUNT) {
		UserList[UserIndex].Counters.IdleCount = 0;

		/* 'Is the user logged? */
		if (UserList[UserIndex].flags.UserLogged) {
			CloseSocket(UserIndex);
			return;
		}
	}

	/* ' Ante cualquier paquete, pierde la proteccion de ser atacado. */
	UserList[UserIndex].flags.NoPuedeSerAtacado = false;

	try {
		HoAPacketHandler packet(UserIndex);
		hoa::protocol::client::ClientPacketDecodeAndDispatch(
				UserList[UserIndex].incomingData.get(),
				&(packet));
	} catch (const hoa::protocol::PacketDecodingError& e) {
		CerrarUserIndex(UserIndex);
	}
}

hoa::protocol::client::ClientPacketHandler* HoAPacketHandler::getPacketHandlerClientPacket() {
	return &clientPacketHandler;
}

hoa::protocol::clientgm::ClientGMPacketHandler* HoAPacketHandler::getPacketHandlerClientGMPacket() {
	return &clientPacketHandler;
}

hoa::protocol::server::ServerPacketHandler* HoAPacketHandler::getPacketHandlerServerPacket() {
	return nullptr;
}

void HoAClientPacketHandler::handleGMCommands(hoa::protocol::client::GMCommands* p) {
	HoAPacketHandler packet(UserIndex);
	p->composite->dispatch(&packet);
}

using namespace hoa::protocol::client;
using namespace hoa::protocol::clientgm;


/* '' */
/* ' Handles the "Home" message. */
/* ' */

void HoAClientPacketHandler::handleHome(Home* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Budi */
	/* 'Creation Date: 06/01/2010 */
	/* 'Last Modification: 05/06/10 */
	/* 'Pato - 05/06/10: Add the Ucase$ to prevent problems. */
	/* '*************************************************** */
	if (UserList[UserIndex].flags.TargetNpcTipo == eNPCType_Gobernador) {
		setHome(UserIndex, static_cast<eCiudad>(Npclist[UserList[UserIndex].flags.TargetNPC].Ciudad),
				UserList[UserIndex].flags.TargetNPC);
	} else {
		if (UserList[UserIndex].flags.Muerto == 1) {
			/* 'Si es un mapa común y no está en cana */
			if ((MapInfo[UserList[UserIndex].Pos.Map].Restringir == eRestrict_restrict_no)
					&& (UserList[UserIndex].Counters.Pena == 0)) {
				if (UserList[UserIndex].flags.Traveling == 0) {
					if (Ciudades[UserList[UserIndex].Hogar].Map != UserList[UserIndex].Pos.Map) {
						goHome(UserIndex);
					} else {
						WriteConsoleMsg(UserIndex, "You are already home.",
								FontTypeNames_FONTTYPE_INFO);
					}
				} else {
					WriteMultiMessage(UserIndex, eMessages_CancelHome);
					UserList[UserIndex].flags.Traveling = 0;
					UserList[UserIndex].Counters.goHome = 0;
				}
			} else {
				WriteConsoleMsg(UserIndex, "You cannot use that command here.", FontTypeNames_FONTTYPE_FIGHT);
			}
		} else {
			WriteConsoleMsg(UserIndex, "You must be dead to use that command.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handles the "ThrowDices" message. */
/* ' */


void HoAClientPacketHandler::handleThrowDices() {
	/* '*************************************************** */
	/* 'Last Modification: 05/17/06 */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* ' */
	/* '*************************************************** */

	UserList[UserIndex].Stats.UserAtributos[eAtributos_Fuerza] = MaximoInt(15,
			12 + RandomNumber(0, 3) + RandomNumber(0, 3));
	UserList[UserIndex].Stats.UserAtributos[eAtributos_Agilidad] = MaximoInt(15,
			12 + RandomNumber(0, 3) + RandomNumber(0, 3));
	UserList[UserIndex].Stats.UserAtributos[eAtributos_Inteligencia] = MaximoInt(15,
			12 + RandomNumber(0, 3) + RandomNumber(0, 3));
	UserList[UserIndex].Stats.UserAtributos[eAtributos_Carisma] = MaximoInt(15,
			12 + RandomNumber(0, 3) + RandomNumber(0, 3));
	UserList[UserIndex].Stats.UserAtributos[eAtributos_Constitucion] = MaximoInt(15,
			12 + RandomNumber(0, 3) + RandomNumber(0, 3));
}


void HoAClientPacketHandler::handleLoginExistingChar(LoginExistingChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	std::string& token = p->token;
	std::string& nft_address = p->nft_address;
	zmq::context_t context (2);
    zmq::socket_t sock (context, zmq::socket_type::req);
	nlohmann::json msg;

	msg["id"] = "is_token_active";
	msg["token"] = token;
    msg["nft_address"] = nft_address;

	std::string msg_str = msg.dump();
	zmq::message_t message(msg_str.size());
	std::memcpy (message.data(), msg_str.data(), msg_str.size());

	sock.connect("tcp://127.0.0.1:5555");
	sock.send(message, zmq::send_flags::dontwait);
	zmq::message_t reply{};
    sock.recv(reply, zmq::recv_flags::none);
	auto status = nlohmann::json::parse(reply.to_string());

	if (status["status"] != "OK") {
		WriteErrorMsg(UserIndex, "Invalid signature.");
		FlushBuffer(UserIndex);
		CloseSocket(UserIndex);

		return;
	}

	std::string character_name = GetVar(GetDatPath(DATPATH::Names), "Names", nft_address);

	if (!PersonajeExiste(character_name)) {
		if (PuedeCrearPersonajes == 0) {
			WriteErrorMsg(UserIndex, "Character creation has been disabled in this server.");
			FlushBuffer(UserIndex);
			CloseSocket(UserIndex);

			return;
		}

		if (ServerSoloGMs != 0) {
			WriteErrorMsg(UserIndex, "Server restricted to admins.");
			FlushBuffer(UserIndex);
			CloseSocket(UserIndex);

			return;
		}

		eRaza race;
		eGenero sex;
		eCiudad homeland;
		eClass character_class;
		int head;
		int armor;
		int weapon;
		int helmet;
		int shield;
		int level;
		int slot = 7;

		std::string name;
		std::string mail;

		nlohmann::json msg_char_metadata;

		msg_char_metadata["id"] = "get_nft_metadata";
		msg_char_metadata["nft_address"] = nft_address;

		std::string msg_char_metadata_str = msg_char_metadata.dump();
		zmq::message_t message_metadata(msg_char_metadata_str.size());
		std::memcpy (message_metadata.data(), msg_char_metadata_str.data(), msg_char_metadata_str.size());

		sock.send(message_metadata, zmq::send_flags::dontwait);
		zmq::message_t reply_metadata{};
		sock.recv(reply_metadata, zmq::recv_flags::none);
		auto status_metadata = nlohmann::json::parse(reply_metadata.to_string());

		if (status_metadata["status"] == "OK" && status_metadata["ret"]["is_hoa"] == true) {
			race = static_cast<eRaza>(status_metadata["ret"]["race"]);
			sex = static_cast<eGenero>(status_metadata["ret"]["sex"]);
			character_class = static_cast<eClass>(status_metadata["ret"]["class"]);

			head = status_metadata["ret"]["head"];
			armor = status_metadata["ret"]["body"];
			weapon = status_metadata["ret"]["weapon"];
			helmet = status_metadata["ret"]["helmet"];
			shield = status_metadata["ret"]["shield"];
			name = status_metadata["ret"]["name"];

			level = std::stoi(static_cast<std::string>(status_metadata["ret"]["Level"]));

			if (level > LimiteNewbie) {
				slot = 1;
			}

			mail = "a@a.com";
			homeland = static_cast<eCiudad>(1);

			WriteVar(GetDatPath(DATPATH::Names), "NAMES", nft_address, name);
			handleThrowDices();
						
			ConnectNewUser(UserIndex, name, race, sex, character_class, mail, homeland, head);

			while (UserList[UserIndex].Stats.ELV < level) {
				UserList[UserIndex].Stats.Exp = UserList[UserIndex].Stats.ELU;
				CheckUserLevel(UserIndex);
			}

			UserList[UserIndex].Stats.GLD = level * GOLD_PER_LEVEL;

			Obj armor_obj = Obj();
			armor_obj.ObjIndex = armor;
			armor_obj.Amount = 1;
			MeterItemEnInventario(UserIndex, armor_obj);
			EquiparInvItem(UserIndex, slot);
			slot += 1;

			if (weapon != -1) {
				Obj weapon_obj = Obj();
				weapon_obj.ObjIndex = weapon;
				weapon_obj.Amount = 1;

				MeterItemEnInventario(UserIndex, weapon_obj);
				EquiparInvItem(UserIndex, slot);
				slot += 1;
			}

			if (helmet != -1) {
				Obj helmet_obj = Obj();
				helmet_obj.ObjIndex = helmet;
				helmet_obj.Amount = 1;

				MeterItemEnInventario(UserIndex, helmet_obj);
				EquiparInvItem(UserIndex, slot);
				slot += 1;
			}

			if (shield != -1) {
				Obj shield_obj = Obj();
				shield_obj.ObjIndex = shield;
				shield_obj.Amount = 1;

				MeterItemEnInventario(UserIndex, shield_obj);
				EquiparInvItem(UserIndex, slot);
				slot += 1;
			}
		}
	} else {
		if (BANCheck(character_name)) {
			WriteErrorMsg(UserIndex,
					"You've been banned from Heroes of Argentum");
		} else {
			ConnectUser(UserIndex, character_name);
		}
	}
}

/* '' */
/* ' Handles the "Talk" message. */
/* ' */


void HoAClientPacketHandler::handleTalk(Talk* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 13/01/2010 */
	/* '15/07/2009: ZaMa - Now invisible admins talk by console. */
	/* '23/09/2009: ZaMa - Now invisible admins can't send empty chat. */
	/* '13/01/2010: ZaMa - Now hidden on boat pirats recover the proper boat body. */
	/* '*************************************************** */
	std::string& Chat = p->Chat;

	/* '[Consejeros & GMs] */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero, PlayerType_SemiDios)) {
		LogGM(UserList[UserIndex].Name, "Said: " + Chat);
	}

	/* 'I see you.... */
	if (UserList[UserIndex].flags.Oculto > 0) {
		UserList[UserIndex].flags.Oculto = 0;
		UserList[UserIndex].Counters.TiempoOculto = 0;

		if (UserList[UserIndex].flags.Navegando == 1) {
			if (UserList[UserIndex].clase == eClass_Pirat) {
				/* ' Pierde la apariencia de fragata fantasmal */
				ToggleBoatBody(UserIndex);
				WriteConsoleMsg(UserIndex, "Your appearance is back to normal!",
						FontTypeNames_FONTTYPE_INFO);
				ChangeUserChar(UserIndex, UserList[UserIndex].Char.body, UserList[UserIndex].Char.Head,
						UserList[UserIndex].Char.heading, NingunArma, NingunEscudo, NingunCasco);
			}
		} else {
			if (UserList[UserIndex].flags.invisible == 0) {
				SetInvisible(UserIndex, UserList[UserIndex].Char.CharIndex, false);
				WriteConsoleMsg(UserIndex, "You are now visible again!", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}

	if (vb6::LenB(Chat) != 0) {
		/* 'Analize chat... */
		ParseChat(Chat);

		if (!(UserList[UserIndex].flags.AdminInvisible == 1)) {
			if (UserList[UserIndex].flags.Muerto == 1) {
				SendData(SendTarget_ToDeadArea, UserIndex,
						BuildChatOverHead(Chat, UserList[UserIndex].Char.CharIndex,
								CHAT_COLOR_DEAD_CHAR));
			} else {
				SendData(SendTarget_ToPCArea, UserIndex,
						BuildChatOverHead(Chat, UserList[UserIndex].Char.CharIndex,
								UserList[UserIndex].flags.ChatColor));
			}
		} else {
			if (vb6::RTrim(Chat) != "") {
				SendData(SendTarget_ToPCArea, UserIndex,
						hoa::protocol::server::BuildConsoleMsg("Gm> " + Chat, FontTypeNames_FONTTYPE_GM));
			}
		}
	}



}

/* '' */
/* ' Handles the "Yell" message. */
/* ' */


void HoAClientPacketHandler::handleYell(Yell* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 13/01/2010 (ZaMa) */
	/* '15/07/2009: ZaMa - Now invisible admins yell by console. */
	/* '13/01/2010: ZaMa - Now hidden on boat pirats recover the proper boat body. */
	/* '*************************************************** */
	std::string& Chat = p->Chat;

	/* '[Consejeros & GMs] */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero, PlayerType_SemiDios)) {
		LogGM(UserList[UserIndex].Name, "Grito: " + Chat);
	}

	/* 'I see you.... */
	if (UserList[UserIndex].flags.Oculto > 0) {
		UserList[UserIndex].flags.Oculto = 0;
		UserList[UserIndex].Counters.TiempoOculto = 0;

		if (UserList[UserIndex].flags.Navegando == 1) {
			if (UserList[UserIndex].clase == eClass_Pirat) {
				/* ' Pierde la apariencia de fragata fantasmal */
				ToggleBoatBody(UserIndex);
				WriteConsoleMsg(UserIndex, "Your appearance is back to normal!",
						FontTypeNames_FONTTYPE_INFO);
				ChangeUserChar(UserIndex, UserList[UserIndex].Char.body, UserList[UserIndex].Char.Head,
						UserList[UserIndex].Char.heading, NingunArma, NingunEscudo, NingunCasco);
			}
		} else {
			if (UserList[UserIndex].flags.invisible == 0) {
				SetInvisible(UserIndex, UserList[UserIndex].Char.CharIndex, false);
				WriteConsoleMsg(UserIndex, "You are now visible again!", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}

	if (vb6::LenB(Chat) != 0) {
		/* 'Analize chat... */
		ParseChat(Chat);

		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
			if (UserList[UserIndex].flags.Muerto == 1) {
				SendData(SendTarget_ToDeadArea, UserIndex,
						BuildChatOverHead(Chat, UserList[UserIndex].Char.CharIndex,
								CHAT_COLOR_DEAD_CHAR));
			} else {
				SendData(SendTarget_ToPCArea, UserIndex,
						BuildChatOverHead(Chat, UserList[UserIndex].Char.CharIndex, vbRed));
			}
		} else {
			if (!(UserList[UserIndex].flags.AdminInvisible == 1)) {
				SendData(SendTarget_ToPCArea, UserIndex,
						BuildChatOverHead(Chat, UserList[UserIndex].Char.CharIndex,
								CHAT_COLOR_GM_YELL));
			} else {
				SendData(SendTarget_ToPCArea, UserIndex,
						hoa::protocol::server::BuildConsoleMsg("Gm> " + Chat, FontTypeNames_FONTTYPE_GM));
			}
		}
	}



}

/* '' */
/* ' Handles the "Whisper" message. */
/* ' */


void HoAClientPacketHandler::handleWhisper(Whisper* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 03/12/2010 */
	/* '28/05/2009: ZaMa - Now it doesn't appear any message when private talking to an invisible admin */
	/* '15/07/2009: ZaMa - Now invisible admins wisper by console. */
	/* '03/12/2010: Enanoh - Agregué susurro a Admins en modo consulta y Los Dioses pueden susurrar en ciertos casos. */
	/* '*************************************************** */

	std::string& Chat = p->Chat;
	int TargetUserIndex;
	std::string& TargetName = p->TargetName;

	if (UserList[UserIndex].flags.Muerto) {
		WriteConsoleMsg(UserIndex,
				"You're dead!! The dead cannot communicate with the world of the living. ",
				FontTypeNames_FONTTYPE_INFO);
	} else {
		/* ' Offline? */
		TargetUserIndex = NameIndex(TargetName);
		if (TargetUserIndex == INVALID_INDEX) {
			/* ' Admin? */
			if (EsGmChar(TargetName)) {
				WriteConsoleMsg(UserIndex, "You cannot whisper to admins",
						FontTypeNames_FONTTYPE_INFO);
				/* ' Whisperer admin? (Else say nothing) */
			} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
				WriteConsoleMsg(UserIndex, "Unknown user.", FontTypeNames_FONTTYPE_INFO);
			}

			/* ' Online */
		} else {
			/* ' Consejeros, semis y usuarios no pueden susurrar a dioses (Salvo en consulta) */
			if (UserTieneAlgunPrivilegios(TargetUserIndex, PlayerType_Dios, PlayerType_Admin)
					&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)
					&& !UserList[UserIndex].flags.EnConsulta) {

				/* ' No puede */
				WriteConsoleMsg(UserIndex, "You cannot whisper to admins",
						FontTypeNames_FONTTYPE_INFO);

				/* ' Usuarios no pueden susurrar a semis o conses (Salvo en consulta) */
			} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User) && !UserTieneAlgunPrivilegios(TargetUserIndex, PlayerType_User)
					&& !UserList[UserIndex].flags.EnConsulta) {

				/* ' No puede */
				WriteConsoleMsg(UserIndex, "You cannot whisper to admins",
						FontTypeNames_FONTTYPE_INFO);

				/* ' En rango? (Los dioses pueden susurrar a distancia) */
			} else if (!EstaPCarea(UserIndex, TargetUserIndex)
					&& !UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {

				/* ' No se puede susurrar a admins fuera de su rango */
				if (!UserTieneAlgunPrivilegios(TargetUserIndex, PlayerType_User)
						&& !UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
					WriteConsoleMsg(UserIndex, "You cannot whisper to admins",
							FontTypeNames_FONTTYPE_INFO);

					/* ' Whisperer admin? (Else say nothing) */
				} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
					WriteConsoleMsg(UserIndex, "You are too far away from that user.", FontTypeNames_FONTTYPE_INFO);
				}
			} else {
				/* '[Consejeros & GMs] */
				if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero, PlayerType_SemiDios)) {
					LogGM(UserList[UserIndex].Name,
							"Whispered '" + UserList[TargetUserIndex].Name + "' " + Chat);

					/* ' Usuarios a administradores */
				} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User) && UserTieneAlgunPrivilegios(TargetUserIndex,  PlayerType_User)) {
					LogGM(UserList[TargetUserIndex].Name,
							UserList[UserIndex].Name + " whispered in support: " + Chat);
				}

				if (vb6::LenB(Chat) != 0) {
					/* 'Analize chat... */
					ParseChat(Chat);

					/* ' Dios susurrando a distancia */
					if (!EstaPCarea(UserIndex, TargetUserIndex)
							&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {

						WriteConsoleMsg(UserIndex, "Whispered> " + Chat, FontTypeNames_FONTTYPE_GM);
						WriteConsoleMsg(TargetUserIndex, "GM whispers> " + Chat, FontTypeNames_FONTTYPE_GM);

					} else if (!(UserList[UserIndex].flags.AdminInvisible == 1)) {
						WriteChatOverHead(UserIndex, Chat, UserList[UserIndex].Char.CharIndex, vbBlue);
						WriteChatOverHead(TargetUserIndex, Chat, UserList[UserIndex].Char.CharIndex, vbBlue);
						FlushBuffer(TargetUserIndex);

						/* '[CDT 17-02-2004] */
						if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
							SendData(SendTarget_ToAdminsAreaButConsejeros, UserIndex,
									BuildChatOverHead(
											"A " + UserList[TargetUserIndex].Name + "> " + Chat,
											UserList[UserIndex].Char.CharIndex, vbYellow));
						}
					} else {
						WriteConsoleMsg(UserIndex, "Whispered> " + Chat, FontTypeNames_FONTTYPE_GM);
						if (UserIndex != TargetUserIndex) {
							WriteConsoleMsg(TargetUserIndex, "GM whispers> " + Chat,
									FontTypeNames_FONTTYPE_GM);
						}

						if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
							SendData(SendTarget_ToAdminsAreaButConsejeros, UserIndex,
									hoa::protocol::server::BuildConsoleMsg(
											"GM said to " + UserList[TargetUserIndex].Name + "> " + Chat,
											FontTypeNames_FONTTYPE_GM));
						}
					}
				}
			}
		}
	}



}

/* '' */
/* ' Handles the "Walk" message. */
/* ' */


void HoAClientPacketHandler::handleWalk(Walk* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 13/01/2010 (ZaMa) */
	/* '11/19/09 Pato - Now the class bandit can walk hidden. */
	/* '13/01/2010: ZaMa - Now hidden on boat pirats recover the proper boat body. */
	/* '*************************************************** */

	int dummy;
	int TempTick;
	eHeading heading;

	heading = static_cast<eHeading>(p->Heading);

	/* 'Prevent SpeedHack */
	if (UserList[UserIndex].flags.TimesWalk >= 30) {
		TempTick = vb6::GetTickCount();
		dummy = getInterval(TempTick, UserList[UserIndex].flags.StartWalk);

		/* ' 5800 is actually less than what would be needed in perfect conditions to take 30 steps */
		/* '(it's about 193 ms per step against the over 200 needed in perfect conditions) */
		if (dummy < 5800) {
			if (getInterval(TempTick, UserList[UserIndex].flags.CountSH) > 30000) {
				UserList[UserIndex].flags.CountSH = 0;
			}

			if (UserList[UserIndex].flags.CountSH != 0) {
				if (dummy != 0) {
					dummy = 126000 / dummy;
				}

				LogHackAttemp("Tramposo SH: " + UserList[UserIndex].Name + " , " + vb6::CStr(dummy));
				SendData(SendTarget_ToAdmins, 0,
						hoa::protocol::server::BuildConsoleMsg(
								"Server> " + UserList[UserIndex].Name
										+ " you've been kicked out of the server for the possible use of SH.",
								FontTypeNames_FONTTYPE_SERVER));
				CloseSocket(UserIndex);

				return;
			} else {
				UserList[UserIndex].flags.CountSH = TempTick;
			}
		}
		UserList[UserIndex].flags.StartWalk = TempTick;
		UserList[UserIndex].flags.TimesWalk = 0;
	}

	UserList[UserIndex].flags.TimesWalk = UserList[UserIndex].flags.TimesWalk + 1;

	/* 'If exiting, cancel */
	CancelExit(UserIndex);

	/* 'TODO: Debería decirle por consola que no puede? */
	/* 'Esta usando el /HOGAR, no se puede mover */
	if (UserList[UserIndex].flags.Traveling == 1) {
		return;
	}

	if (UserList[UserIndex].flags.Paralizado == 0) {
		if (UserList[UserIndex].flags.Meditando) {
			/* 'Stop meditating, next action will start movement. */
			UserList[UserIndex].flags.Meditando = false;
			UserList[UserIndex].Char.FX = 0;
			UserList[UserIndex].Char.loops = 0;

			WriteMeditateToggle(UserIndex);
			WriteConsoleMsg(UserIndex, "You stopped meditating.", FontTypeNames_FONTTYPE_INFO);

			SendData(SendTarget_ToPCArea, UserIndex,
					hoa::protocol::server::BuildCreateFX(UserList[UserIndex].Char.CharIndex, 0, 0));
		} else {
			/* 'Move user */
			MoveUserChar(UserIndex, heading);

			/* 'Stop resting if needed */
			if (UserList[UserIndex].flags.Descansar) {
				UserList[UserIndex].flags.Descansar = false;

				WriteRestOK(UserIndex);
				WriteConsoleMsg(UserIndex, "You stopped resting.", FontTypeNames_FONTTYPE_INFO);
			}
		}
		/* 'paralized */
	} else {
		if (UserList[UserIndex].flags.UltimoMensaje != 1) {
			UserList[UserIndex].flags.UltimoMensaje = 1;

			WriteConsoleMsg(UserIndex, "You cannot move because you're paralyzed.",
					FontTypeNames_FONTTYPE_INFO);
		}

		UserList[UserIndex].flags.CountSH = 0;
	}

	/* 'Can't move while hidden except he is a thief */
	if (UserList[UserIndex].flags.Oculto == 1 && UserList[UserIndex].flags.AdminInvisible == 0) {
		if (UserList[UserIndex].clase != eClass_Thief && UserList[UserIndex].clase != eClass_Bandit) {
			UserList[UserIndex].flags.Oculto = 0;
			UserList[UserIndex].Counters.TiempoOculto = 0;

			if (UserList[UserIndex].flags.Navegando == 1) {
				if (UserList[UserIndex].clase == eClass_Pirat) {
					/* ' Pierde la apariencia de fragata fantasmal */
					ToggleBoatBody(UserIndex);
					WriteConsoleMsg(UserIndex, "Your appearance is back to normal!",
							FontTypeNames_FONTTYPE_INFO);
					ChangeUserChar(UserIndex, UserList[UserIndex].Char.body, UserList[UserIndex].Char.Head,
							UserList[UserIndex].Char.heading, NingunArma, NingunEscudo, NingunCasco);
				}
			} else {
				/* 'If not under a spell effect, show char */
				if (UserList[UserIndex].flags.invisible == 0) {
					WriteConsoleMsg(UserIndex, "You are now visible again.", FontTypeNames_FONTTYPE_INFO);
					SetInvisible(UserIndex, UserList[UserIndex].Char.CharIndex, false);
				}
			}
		}
	}
}

/* '' */
/* ' Handles the "RequestPositionUpdate" message. */
/* ' */


void HoAClientPacketHandler::handleRequestPositionUpdate(RequestPositionUpdate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	WritePosUpdate(UserIndex);
}

/* '' */
/* ' Handles the "Attack" message. */
/* ' */


void HoAClientPacketHandler::handleAttack(Attack* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 13/01/2010 */
	/* 'Last Modified By: ZaMa */
	/* '10/01/2008: Tavo - Se cancela la salida del juego si el user esta saliendo. */
	/* '13/11/2009: ZaMa - Se cancela el estado no atacable al atcar. */
	/* '13/01/2010: ZaMa - Now hidden on boat pirats recover the proper boat body. */
	/* '*************************************************** */
	/* 'If dead, can't attack */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'If user meditates, can't attack */
	if (UserList[UserIndex].flags.Meditando) {
		return;
	}

	/* 'If equiped weapon is ranged, can't attack this way */
	if (UserList[UserIndex].Invent.WeaponEqpObjIndex > 0) {
		if (ObjData[UserList[UserIndex].Invent.WeaponEqpObjIndex].proyectil == 1) {
			WriteConsoleMsg(UserIndex, "No puedes usar así este arma.", FontTypeNames_FONTTYPE_INFO);
			return;
		}
	}

	/* 'Admins can't attack. */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	/* 'If exiting, cancel */
	CancelExit(UserIndex);

	/* 'Attack! */
	UsuarioAtaca(UserIndex);

	/* 'Now you can be atacked */
	UserList[UserIndex].flags.NoPuedeSerAtacado = false;

	/* 'I see you... */
	if (UserList[UserIndex].flags.Oculto > 0 && UserList[UserIndex].flags.AdminInvisible == 0) {
		UserList[UserIndex].flags.Oculto = 0;
		UserList[UserIndex].Counters.TiempoOculto = 0;

		if (UserList[UserIndex].flags.Navegando == 1) {
			if (UserList[UserIndex].clase == eClass_Pirat) {
				/* ' Pierde la apariencia de fragata fantasmal */
				ToggleBoatBody(UserIndex);
				WriteConsoleMsg(UserIndex, "Your appearance is back to normal!",
						FontTypeNames_FONTTYPE_INFO);
				ChangeUserChar(UserIndex, UserList[UserIndex].Char.body, UserList[UserIndex].Char.Head,
						UserList[UserIndex].Char.heading, NingunArma, NingunEscudo, NingunCasco);
			}
		} else {
			if (UserList[UserIndex].flags.invisible == 0) {
				SetInvisible(UserIndex, UserList[UserIndex].Char.CharIndex, false);
				WriteConsoleMsg(UserIndex, "You are now visible again!", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}
}

/* '' */
/* ' Handles the "PickUp" message. */
/* ' */


void HoAClientPacketHandler::handlePickUp(PickUp* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 07/25/09 */
	/* '02/26/2006: Marco - Agregué un checkeo por si el usuario trata de agarrar un item mientras comercia. */
	/* '*************************************************** */

	/* 'If dead, it can't pick up objects */
	if (UserList[UserIndex].flags.Muerto == 1) {
		return;
	}

	/* 'If user is trading items and attempts to pickup an item, he's cheating, so we kick him. */
	if (UserList[UserIndex].flags.Comerciando) {
		return;
	}

	/* 'Lower rank administrators can't pick up items */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
		if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)) {
			WriteConsoleMsg(UserIndex, "You cannot pick up any objects.", FontTypeNames_FONTTYPE_INFO);
			return;
		}
	}

	GetObj(UserIndex);
}

/* '' */
/* ' Handles the "SafeToggle" message. */
/* ' */


void HoAClientPacketHandler::handleSafeToggle(SafeToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	if (UserList[UserIndex].flags.Seguro) {
		/* 'Call WriteSafeModeOff(UserIndex) */
		WriteMultiMessage(UserIndex, eMessages_SafeModeOff);
	} else {
		/* 'Call WriteSafeModeOn(UserIndex) */
		WriteMultiMessage(UserIndex, eMessages_SafeModeOn);
	}

	UserList[UserIndex].flags.Seguro = !UserList[UserIndex].flags.Seguro;
}

/* '' */
/* ' Handles the "ResuscitationSafeToggle" message. */
/* ' */


void HoAClientPacketHandler::handleResuscitationSafeToggle(ResuscitationSafeToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Rapsodius */
	/* 'Creation Date: 10/10/07 */
	/* '*************************************************** */

	UserList[UserIndex].flags.SeguroResu = !UserList[UserIndex].flags.SeguroResu;

	if (UserList[UserIndex].flags.SeguroResu) {
		/* 'Call WriteResuscitationSafeOn(UserIndex) */
		WriteMultiMessage(UserIndex, eMessages_ResuscitationSafeOn);
	} else {
		/* 'Call WriteResuscitationSafeOff(UserIndex) */
		WriteMultiMessage(UserIndex, eMessages_ResuscitationSafeOff);
	}
}

/* '' */
/* ' Handles the "RequestGuildLeaderInfo" message. */
/* ' */


void HoAClientPacketHandler::handleRequestGuildLeaderInfo(RequestGuildLeaderInfo* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	SendGuildLeaderInfo(UserIndex);
}

/* '' */
/* ' Handles the "RequestAtributes" message. */
/* ' */


void HoAClientPacketHandler::handleRequestAtributes(RequestAtributes* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	WriteAttributes(UserIndex);
}

/* '' */
/* ' Handles the "RequestFame" message. */
/* ' */


void HoAClientPacketHandler::handleRequestFame(RequestFame* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	EnviarFama(UserIndex);
}

/* '' */
/* ' Handles the "RequestSkills" message. */
/* ' */


void HoAClientPacketHandler::handleRequestSkills(RequestSkills* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	WriteSendSkills(UserIndex);
}

/* '' */
/* ' Handles the "RequestMiniStats" message. */
/* ' */


void HoAClientPacketHandler::handleRequestMiniStats(RequestMiniStats* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	WriteMiniStats(UserIndex);
}

/* '' */
/* ' Handles the "CommerceEnd" message. */
/* ' */


void HoAClientPacketHandler::handleCommerceEnd(CommerceEnd* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	/* 'User quits commerce mode */
	UserList[UserIndex].flags.Comerciando = false;
	WriteCommerceEnd(UserIndex);
}

/* '' */
/* ' Handles the "UserCommerceEnd" message. */
/* ' */


void HoAClientPacketHandler::handleUserCommerceEnd(UserCommerceEnd* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 11/03/2010 */
	/* '11/03/2010: ZaMa - Le avisa por consola al que cencela que dejo de comerciar. */
	/* '*************************************************** */

	/* 'Quits commerce mode with user */
	if (UserList[UserIndex].ComUsu.DestUsu > 0) {
		if (UserList[UserList[UserIndex].ComUsu.DestUsu].ComUsu.DestUsu == UserIndex) {
			WriteConsoleMsg(UserList[UserIndex].ComUsu.DestUsu,
					UserList[UserIndex].Name + " has stopped trading with you.",
					FontTypeNames_FONTTYPE_TALK);
			FinComerciarUsu(UserList[UserIndex].ComUsu.DestUsu);

			/* 'Send data in the outgoing buffer of the other user */
			FlushBuffer(UserList[UserIndex].ComUsu.DestUsu);
		}
	}

	FinComerciarUsu(UserIndex);
	WriteConsoleMsg(UserIndex, "You've stopped trading.", FontTypeNames_FONTTYPE_TALK);
}

/* '' */
/* ' Handles the "UserCommerceConfirm" message. */
/* ' */

void HoAClientPacketHandler::handleUserCommerceConfirm(UserCommerceConfirm* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/12/2009 */
	/* ' */
	/* '*************************************************** */




	/* 'Validate the commerce */
	if (PuedeSeguirComerciando(UserIndex)) {
		/* 'Tell the other user the confirmation of the offer */
		WriteUserOfferConfirm(UserList[UserIndex].ComUsu.DestUsu);
		UserList[UserIndex].ComUsu.Confirmo = true;
	}

}

void HoAClientPacketHandler::handleCommerceChat(CommerceChat* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 03/12/2009 */
	/* ' */
	/* '*************************************************** */

	std::string& Chat = p->Chat;

	if (vb6::LenB(Chat) != 0) {
		if (PuedeSeguirComerciando(UserIndex)) {
			/* 'Analize chat... */
			ParseChat(Chat);

			Chat = UserList[UserIndex].Name + "> " + Chat;
			WriteCommerceChat(UserIndex, Chat, FontTypeNames_FONTTYPE_PARTY);
			WriteCommerceChat(UserList[UserIndex].ComUsu.DestUsu, Chat, FontTypeNames_FONTTYPE_PARTY);
		}
	}




}

/* '' */
/* ' Handles the "BankEnd" message. */
/* ' */


void HoAClientPacketHandler::handleBankEnd(BankEnd* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	/* 'User exits banking mode */
	UserList[UserIndex].flags.Comerciando = false;
	WriteBankEnd(UserIndex);
}

/* '' */
/* ' Handles the "UserCommerceOk" message. */
/* ' */


void HoAClientPacketHandler::handleUserCommerceOk(UserCommerceOk* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	/* 'Trade accepted */
	AceptarComercioUsu(UserIndex);
}

/* '' */
/* ' Handles the "UserCommerceReject" message. */
/* ' */


void HoAClientPacketHandler::handleUserCommerceReject(UserCommerceReject* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int otherUser;

	otherUser = UserList[UserIndex].ComUsu.DestUsu;

	/* 'Offer rejected */
	if (otherUser > 0) {
		if (UserList[otherUser].flags.UserLogged) {
			WriteConsoleMsg(otherUser, UserList[UserIndex].Name + " has rejected your offer.",
					FontTypeNames_FONTTYPE_TALK);
			FinComerciarUsu(otherUser);

			/* 'Send data in the outgoing buffer of the other user */
			FlushBuffer(otherUser);
		}
	}

	WriteConsoleMsg(UserIndex, "You've rejected the user's offer.", FontTypeNames_FONTTYPE_TALK);
	FinComerciarUsu(UserIndex);
}

/* '' */
/* ' Handles the "Drop" message. */
/* ' */


void HoAClientPacketHandler::handleDrop(Drop* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 07/25/09 */
	/* '07/25/09: Marco - Agregué un checkeo para patear a los usuarios que tiran items mientras comercian. */
	/* '*************************************************** */

	int Slot;
	int Amount;

	Slot = p->Slot;
	Amount = p->Amount;

	/* 'low rank admins can't drop item. Neither can the dead nor those sailing. */
	if (UserList[UserIndex].flags.Navegando == 1 || UserList[UserIndex].flags.Muerto == 1
			|| (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)
					&& !UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster))) {
		return;
	}

	/* 'If the user is trading, he can't drop items => He's cheating, we kick him. */
	if (UserList[UserIndex].flags.Comerciando) {
		return;
	}

	/* 'Are we dropping gold or other items?? */
	if (Slot == FLAGORO) {
		/* 'Don't drop too much gold */
		if (Amount > 10000) {
			return;
		}

		TirarOro(Amount, UserIndex);

		WriteUpdateGold(UserIndex);
	} else {
		/* 'Only drop valid slots */
		if (Slot <= MAX_INVENTORY_SLOTS && Slot > 0) {
			if (UserList[UserIndex].Invent.Object[Slot].ObjIndex == 0) {
				return;
			}

			DropObj(UserIndex, Slot, Amount, UserList[UserIndex].Pos.Map, UserList[UserIndex].Pos.X,
					UserList[UserIndex].Pos.Y, true);
		}
	}
}

/* '' */
/* ' Handles the "CastSpell" message. */
/* ' */


void HoAClientPacketHandler::handleCastSpell(CastSpell* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* '13/11/2009: ZaMa - Ahora los npcs pueden atacar al usuario si quizo castear un hechizo */
	/* '*************************************************** */

	int Spell;

	Spell = p->Spell;

	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Now you can be atacked */
	UserList[UserIndex].flags.NoPuedeSerAtacado = false;

	if (Spell < 1) {
		UserList[UserIndex].flags.Hechizo = 0;
		return;
	} else if (Spell > MAXUSERHECHIZOS) {
		UserList[UserIndex].flags.Hechizo = 0;
		return;
	}

	UserList[UserIndex].flags.Hechizo = UserList[UserIndex].Stats.UserHechizos[Spell];
}

/* '' */
/* ' Handles the "LeftClick" message. */
/* ' */


void HoAClientPacketHandler::handleLeftClick(LeftClick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	LookatTile(UserIndex, UserList[UserIndex].Pos.Map, p->X, p->Y);
}

/* '' */
/* ' Handles the "DoubleClick" message. */
/* ' */


void HoAClientPacketHandler::handleDoubleClick(DoubleClick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	Accion(UserIndex, UserList[UserIndex].Pos.Map, p->X, p->Y);
}

/* '' */
/* ' Handles the "Work" message. */
/* ' */


void HoAClientPacketHandler::handleWork(Work* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 13/01/2010 (ZaMa) */
	/* '13/01/2010: ZaMa - El pirata se puede ocultar en barca */
	/* '*************************************************** */

	eSkill Skill = static_cast<eSkill>( p->Skill );

	if (UserList[UserIndex].flags.Muerto == 1) {
		return;
	}

	/* 'If exiting, cancel */
	CancelExit(UserIndex);

	switch (Skill) {
	case eSkill_Robar:
	case eSkill_Magia:
	case eSkill_Domar:
		WriteMultiMessage(UserIndex, eMessages_WorkRequestTarget, Skill);
		break;

	case eSkill_Ocultarse:

		/* ' Verifico si se peude ocultar en este mapa */
		if (MapInfo[UserList[UserIndex].Pos.Map].OcultarSinEfecto == 1) {
			WriteConsoleMsg(UserIndex, "You cannot hide here!", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (UserList[UserIndex].flags.EnConsulta) {
			WriteConsoleMsg(UserIndex, "You cannot hide while you're on support.",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (UserList[UserIndex].flags.Navegando == 1) {
			if (UserList[UserIndex].clase != eClass_Pirat) {
				/* '[CDT 17-02-2004] */
				if (UserList[UserIndex].flags.UltimoMensaje != 3) {
					WriteConsoleMsg(UserIndex, "You cannot hide while you're sailing.",
							FontTypeNames_FONTTYPE_INFO);
					UserList[UserIndex].flags.UltimoMensaje = 3;
				}
				/* '[/CDT] */
				return;
			}
		}

		if (UserList[UserIndex].flags.Oculto == 1) {
			/* '[CDT 17-02-2004] */
			if (UserList[UserIndex].flags.UltimoMensaje != 2) {
				WriteConsoleMsg(UserIndex, "You're already hiding.", FontTypeNames_FONTTYPE_INFO);
				UserList[UserIndex].flags.UltimoMensaje = 2;
			}
			/* '[/CDT] */
			return;
		}

		DoOcultarse(UserIndex);
		break;

	default:
		break;
	}

}

/* '' */
/* ' Handles the "InitCrafting" message. */
/* ' */


void HoAClientPacketHandler::handleInitCrafting(InitCrafting* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 29/01/2010 */
	/* ' */
	/* '*************************************************** */
	int TotalItems;
	int ItemsPorCiclo;

	TotalItems = p->TotalItems;
	ItemsPorCiclo = p->ItemsPorCiclo;

	if (TotalItems > 0) {
		UserList[UserIndex].Construir.Cantidad = TotalItems;
		UserList[UserIndex].Construir.PorCiclo = MinimoInt(MaxItemsConstruibles(UserIndex), ItemsPorCiclo);
	}
}

/* '' */
/* ' Handles the "UseSpellMacro" message. */
/* ' */


void HoAClientPacketHandler::handleUseSpellMacro(UseSpellMacro* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	SendData(SendTarget_ToAdmins, UserIndex,
			hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + " was expelled by the anti-macro spell system.",
					FontTypeNames_FONTTYPE_VENENO));
	WriteErrorMsg(UserIndex,
			"You've been expelled for using a spells macro.");
	FlushBuffer(UserIndex);
	CloseSocket(UserIndex);
}

/* '' */
/* ' Handles the "UseItem" message. */
/* ' */


void HoAClientPacketHandler::handleUseItem(UseItem* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Slot = p->Slot;

	if (Slot <= UserList[UserIndex].CurrentInventorySlots && Slot > 0) {
		if (UserList[UserIndex].Invent.Object[Slot].ObjIndex == 0) {
			return;
		}
	}

	if (UserList[UserIndex].flags.Meditando) {
		/* 'The error message should have been provided by the client. */
		return;
	}

	/* # IF SeguridadAlkon THEN */
	/* # END IF */

	UseInvItem(UserIndex, Slot);
}

/* '' */
/* ' Handles the "CraftBlacksmith" message. */
/* ' */


void HoAClientPacketHandler::handleCraftBlacksmith(CraftBlacksmith* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Item = p->Item;

	if (Item < 1) {
		return;
	}

	if (ObjData[Item].SkHerreria == 0) {
		return;
	}

	if (!IntervaloPermiteTrabajar(UserIndex)) {
		return;
	}
	HerreroConstruirItem(UserIndex, Item);
}

/* '' */
/* ' Handles the "CraftCarpenter" message. */
/* ' */


void HoAClientPacketHandler::handleCraftCarpenter(CraftCarpenter* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Item = p->Item;

	if (Item < 1) {
		return;
	}

	if (ObjData[Item].SkCarpinteria == 0) {
		return;
	}

	if (!IntervaloPermiteTrabajar(UserIndex)) {
		return;
	}
	CarpinteroConstruirItem(UserIndex, Item);
}

/* '' */
/* ' Handles the "WorkLeftClick" message. */
/* ' */


void HoAClientPacketHandler::handleWorkLeftClick(WorkLeftClick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 14/01/2010 (ZaMa) */
	/* '16/11/2009: ZaMa - Agregada la posibilidad de extraer madera elfica. */
	/* '12/01/2010: ZaMa - Ahora se admiten armas arrojadizas (proyectiles sin municiones). */
	/* '14/01/2010: ZaMa - Ya no se pierden municiones al atacar npcs con dueno. */
	/* '*************************************************** */

	int X;
	int Y;
	eSkill Skill;
	int DummyInt;
	/* 'Target user */
	int tU;
	/* 'Target NPC */
	int tN;

	int WeaponIndex;

	int Map = UserList[UserIndex].Pos.Map;
	X = p->X;
	Y = p->Y;

	Skill = static_cast<eSkill> (p->Skill);

	if (UserList[UserIndex].flags.Muerto == 1 || UserList[UserIndex].flags.Descansar
			|| UserList[UserIndex].flags.Meditando || !InMapBounds(UserList[UserIndex].Pos.Map, X, Y)) {
		return;
	}

	if (!InRangoVision(UserIndex, X, Y)) {
		WritePosUpdate(UserIndex);
		return;
	}

	/* 'If exiting, cancel */
	CancelExit(UserIndex);

	switch (Skill) {
	case eSkill_Proyectiles:

		/* 'Check attack interval */
		if (!IntervaloPermiteAtacar(UserIndex, false)) {
			return;
		}
		/* 'Check Magic interval */
		if (!IntervaloPermiteLanzarSpell(UserIndex, false)) {
			return;
		}
		/* 'Check bow's interval */
		if (!IntervaloPermiteUsarArcos(UserIndex)) {
			return;
		}

		LanzarProyectil(UserIndex, X, Y);

		break;

	case eSkill_Magia:
		/* 'Check the map allows spells to be casted. */
		if (MapInfo[UserList[UserIndex].Pos.Map].MagiaSinEfecto > 0) {
			WriteConsoleMsg(UserIndex, "A dark force prevents you from channeling your energy.",
					FontTypeNames_FONTTYPE_FIGHT);
			return;
		}

		/* 'Target whatever is in that tile */
		LookatTile(UserIndex, UserList[UserIndex].Pos.Map, X, Y);

		/* 'If it's outside range log it and exit */
		if (vb6::Abs(UserList[UserIndex].Pos.X - X) > RANGO_VISION_X
				|| vb6::Abs(UserList[UserIndex].Pos.Y - Y) > RANGO_VISION_Y) {
			LogCheating(
					"Ataque fuera de rango de " + UserList[UserIndex].Name + "(" + vb6::CStr(UserList[UserIndex].Pos.Map)
							+ "/" + vb6::CStr(UserList[UserIndex].Pos.X) + "/" + vb6::CStr(UserList[UserIndex].Pos.Y) + ") ip: "
							+ UserList[UserIndex].ip + " a la posición (" + vb6::CStr(UserList[UserIndex].Pos.Map) + "/"
							+ vb6::CStr(X) + "/" + vb6::CStr(Y) + ")");
			return;
		}

		/* 'Check bow's interval */
		if (!IntervaloPermiteUsarArcos(UserIndex, false)) {
			return;
		}

		/* 'Check Spell-Hit interval */
		if (!IntervaloPermiteGolpeMagia(UserIndex)) {
			/* 'Check Magic interval */
			if (!IntervaloPermiteLanzarSpell(UserIndex)) {
				return;
			}
		}

		/* 'Check intervals and cast */
		if (UserList[UserIndex].flags.Hechizo > 0) {
			LanzarHechizo(UserList[UserIndex].flags.Hechizo, UserIndex);
			UserList[UserIndex].flags.Hechizo = 0;
		} else {
			WriteConsoleMsg(UserIndex, "Pick the spell you want to cast first!",
					FontTypeNames_FONTTYPE_INFO);
		}

		break;

	case eSkill_Pesca:
		if (MapData[Map][X][Y].trigger == eTrigger_ZONASEGURA || MapInfo[Map].Pk == false) {
			WriteConsoleMsg(UserIndex, "You can't fish within safe zones.", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		WeaponIndex = UserList[UserIndex].Invent.WeaponEqpObjIndex;
		if (WeaponIndex == 0) {
			return;
		}

		/* 'Check interval */
		if (!IntervaloPermiteTrabajar(UserIndex)) {
			return;
		}

		/* 'Basado en la idea de Barrin */
		/* 'Comentario por Barrin: jah, "basado", caradura ! ^^ */
		if (MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].trigger
				== 1) {
			WriteConsoleMsg(UserIndex, "You cannot fish where you are.",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (HayAgua(UserList[UserIndex].Pos.Map, X, Y)) {
			if (WeaponIndex == CANA_PESCA || WeaponIndex == CANA_PESCA_NEWBIE) {
				DoPescar(UserIndex);

			} else if (WeaponIndex == RED_PESCA) {
				DummyInt = MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex;

				if (DummyInt == 0) {
					WriteConsoleMsg(UserIndex, "There's no fish there.",
							FontTypeNames_FONTTYPE_INFO);
					return;
				}

				if (vb6::Abs(UserList[UserIndex].Pos.X - X) + vb6::Abs(UserList[UserIndex].Pos.Y - Y) > 2) {
					WriteConsoleMsg(UserIndex, "You're too far away to fish.",
							FontTypeNames_FONTTYPE_INFO);
					return;
				}

				if (UserList[UserIndex].Pos.X == X && UserList[UserIndex].Pos.Y == Y) {
					WriteConsoleMsg(UserIndex, "You can't fish from there.", FontTypeNames_FONTTYPE_INFO);
					return;
				}

				/* '¿Hay un arbol normal donde clickeo? */
				if (ObjData[DummyInt].OBJType == eOBJType_otYacimientoPez) {
					DoPescarRed(UserIndex);
				} else {
					WriteConsoleMsg(UserIndex, "There's no fish there.",
							FontTypeNames_FONTTYPE_INFO);
					return;
				}

				break;
			} else {
				/* 'Invalid item! */
			}

			/* 'Play sound! */
			SendData(SendTarget_ToPCArea, UserIndex,
					hoa::protocol::server::BuildPlayWave(SND_PESCAR, UserList[UserIndex].Pos.X, UserList[UserIndex].Pos.Y));
		} else {
			WriteConsoleMsg(UserIndex, "There's no water where to fish. Look for a body of water.",
					FontTypeNames_FONTTYPE_INFO);
		}
		break;

	case eSkill_Robar:
		/* 'Does the map allow us to steal here? */
		if (MapInfo[UserList[UserIndex].Pos.Map].Pk) {

			/* 'Check interval */
			if (!IntervaloPermiteTrabajar(UserIndex)) {
				return;
			}

			/* 'Target whatever is in that tile */
			LookatTile(UserIndex, UserList[UserIndex].Pos.Map, X, Y);

			tU = UserList[UserIndex].flags.TargetUser;

			if (tU > 0 && tU != UserIndex) {
				/* 'Can't steal administrative players */
				if (UserTieneAlgunPrivilegios(tU, PlayerType_User)) {
					if (UserList[tU].flags.Muerto == 0) {
						if (vb6::Abs(UserList[UserIndex].Pos.X - X) + vb6::Abs(UserList[UserIndex].Pos.Y - Y)
								> 2) {
							WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
							return;
						}

						/* '17/09/02 */
						/* 'Check the trigger */
						if (MapData[UserList[tU].Pos.Map][X][Y].trigger == eTrigger_ZONASEGURA) {
							WriteConsoleMsg(UserIndex, "You can't steal here.",
									FontTypeNames_FONTTYPE_WARNING);
							return;
						}

						if (MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].trigger
								== eTrigger_ZONASEGURA) {
							WriteConsoleMsg(UserIndex, "You can't steal here.",
									FontTypeNames_FONTTYPE_WARNING);
							return;
						}

						DoRobar(UserIndex, tU);
					}
				}
			} else {
				WriteConsoleMsg(UserIndex, "There's no one to steal from!", FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, "You can't steal within safe zones!", FontTypeNames_FONTTYPE_INFO);
		}

		break;

	case eSkill_Talar:
		if (MapData[Map][X][Y].trigger == eTrigger_ZONASEGURA || MapInfo[Map].Pk == false) {
			WriteConsoleMsg(UserIndex, "You can't chop wood within safe zones.", FontTypeNames_FONTTYPE_INFO);
			return;
		}
		/* 'Check interval */
		if (!IntervaloPermiteTrabajar(UserIndex)) {
			return;
		}

		WeaponIndex = UserList[UserIndex].Invent.WeaponEqpObjIndex;

		if (WeaponIndex == 0) {
			WriteConsoleMsg(UserIndex, "You should equip your axe.", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (WeaponIndex != HACHA_LENADOR && WeaponIndex != HACHA_LENA_ELFICA
				&& WeaponIndex != HACHA_LENADOR_NEWBIE) {
			/* ' Podemos llegar acá si el user equipó el anillo dsp de la U y antes del click */
			return;
		}

		DummyInt = MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex;

		if (DummyInt > 0) {
			if (vb6::Abs(UserList[UserIndex].Pos.X - X) + vb6::Abs(UserList[UserIndex].Pos.Y - Y) > 2) {
				WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
				return;
			}

			/* 'Barrin 29/9/03 */
			if (UserList[UserIndex].Pos.X == X && UserList[UserIndex].Pos.Y == Y) {
				WriteConsoleMsg(UserIndex, "You can't chop wood from there.", FontTypeNames_FONTTYPE_INFO);
				return;
			}

			/* '¿Hay un arbol normal donde clickeo? */
			if (ObjData[DummyInt].OBJType == eOBJType_otArboles) {
				if (WeaponIndex == HACHA_LENADOR || WeaponIndex == HACHA_LENADOR_NEWBIE) {
					SendData(SendTarget_ToPCArea, UserIndex,
							hoa::protocol::server::BuildPlayWave(SND_TALAR, UserList[UserIndex].Pos.X,
									UserList[UserIndex].Pos.Y));
					DoTalar(UserIndex);
				} else {
					WriteConsoleMsg(UserIndex, "You can't chop wood from that tree with that axe.",
							FontTypeNames_FONTTYPE_INFO);
				}

				/* ' Arbol Elfico? */
			} else if (ObjData[DummyInt].OBJType == eOBJType_otArbolElfico) {

				if (WeaponIndex == HACHA_LENA_ELFICA) {
					SendData(SendTarget_ToPCArea, UserIndex,
							hoa::protocol::server::BuildPlayWave(SND_TALAR, UserList[UserIndex].Pos.X,
									UserList[UserIndex].Pos.Y));
					DoTalar(UserIndex, true);
				} else {
					WriteConsoleMsg(UserIndex, "Your axe isn't powerful enough.",
							FontTypeNames_FONTTYPE_INFO);
				}
			}
		} else {
			WriteConsoleMsg(UserIndex, "There's no tree there.", FontTypeNames_FONTTYPE_INFO);
		}

		break;

	case eSkill_Mineria:
		if (!IntervaloPermiteTrabajar(UserIndex)) {
			return;
		}

		WeaponIndex = UserList[UserIndex].Invent.WeaponEqpObjIndex;

		if (WeaponIndex == 0) {
			return;
		}

		if (WeaponIndex != PIQUETE_MINERO && WeaponIndex != PIQUETE_MINERO_NEWBIE) {
			/* ' Podemos llegar acá si el user equipó el anillo dsp de la U y antes del click */
			return;
		}

		/* 'Target whatever is in the tile */
		LookatTile(UserIndex, UserList[UserIndex].Pos.Map, X, Y);

		DummyInt = MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex;

		if (DummyInt > 0) {
			/* 'Check distance */
			if (vb6::Abs(UserList[UserIndex].Pos.X - X) + vb6::Abs(UserList[UserIndex].Pos.Y - Y) > 2) {
				WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
				return;
			}

			/* '¿Hay un yacimiento donde clickeo? */
			if (ObjData[DummyInt].OBJType == eOBJType_otYacimiento) {
				DoMineria(UserIndex);
			} else {
				WriteConsoleMsg(UserIndex, "There's no lode there.", FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, "There's no lode there.", FontTypeNames_FONTTYPE_INFO);
		}

		break;

	case eSkill_Domar:
		/* 'Modificado 25/11/02 */
		/* 'Optimizado y solucionado el bug de la doma de */
		/* 'criaturas hostiles. */

		/* 'Target whatever is that tile */
		LookatTile(UserIndex, UserList[UserIndex].Pos.Map, X, Y);
		tN = UserList[UserIndex].flags.TargetNPC;

		if (tN > 0) {
			if (Npclist[tN].flags.Domable > 0) {
				if (vb6::Abs(UserList[UserIndex].Pos.X - X) + vb6::Abs(UserList[UserIndex].Pos.Y - Y) > 2) {
					WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
					return;
				}

				if (vb6::LenB(Npclist[tN].flags.AttackedBy) != 0) {
					WriteConsoleMsg(UserIndex,
							"You can't tame a creature that's fighting a PC.",
							FontTypeNames_FONTTYPE_INFO);
					return;
				}

				DoDomar(UserIndex, tN);
			} else {
				WriteConsoleMsg(UserIndex, "You can't tame that creature.", FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, "There's no creature there!", FontTypeNames_FONTTYPE_INFO);
		}

		/* 'UGLY!!! This is a constant, not a skill!! */
		break;

	case eSkill_FundirMetal:
		/* 'Check interval */
		if (!IntervaloPermiteTrabajar(UserIndex)) {
			return;
		}

		/* 'Check there is a proper item there */
		if (UserList[UserIndex].flags.TargetObj > 0) {
			if (ObjData[UserList[UserIndex].flags.TargetObj].OBJType == eOBJType_otFragua) {
				/* 'Validate other items */
				if (UserList[UserIndex].flags.TargetObjInvSlot < 1
						|| UserList[UserIndex].flags.TargetObjInvSlot
								> UserList[UserIndex].CurrentInventorySlots) {
					return;
				}

				/* ''chequeamos que no se zarpe duplicando oro */
				if (UserList[UserIndex].Invent.Object[UserList[UserIndex].flags.TargetObjInvSlot].ObjIndex
						!= UserList[UserIndex].flags.TargetObjInvIndex) {
					if (UserList[UserIndex].Invent.Object[UserList[UserIndex].flags.TargetObjInvSlot].ObjIndex
							== 0
							|| UserList[UserIndex].Invent.Object[UserList[UserIndex].flags.TargetObjInvSlot].Amount
									== 0) {
						WriteConsoleMsg(UserIndex, "You have no more minerals.", FontTypeNames_FONTTYPE_INFO);
						return;
					}

					/* ''FUISTE */
					WriteErrorMsg(UserIndex, "HYou've been expelled by the anti-cheat system.");
					FlushBuffer(UserIndex);
					CloseSocket(UserIndex);
					return;
				}
				if (ObjData[UserList[UserIndex].flags.TargetObjInvIndex].OBJType == eOBJType_otMinerales) {
					FundirMineral(UserIndex);
				} else if (ObjData[UserList[UserIndex].flags.TargetObjInvIndex].OBJType
						== eOBJType_otWeapon) {
					FundirArmas(UserIndex);
				}
			} else {
				WriteConsoleMsg(UserIndex, "There's no forge there.", FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, "There's no forge there.", FontTypeNames_FONTTYPE_INFO);
		}

		break;

	case eSkill_Herreria:
		/* 'Target wehatever is in that tile */
		LookatTile(UserIndex, UserList[UserIndex].Pos.Map, X, Y);

		if (UserList[UserIndex].flags.TargetObj > 0) {
			if (ObjData[UserList[UserIndex].flags.TargetObj].OBJType == eOBJType_otYunque) {
				EnivarArmasConstruibles(UserIndex);
				EnivarArmadurasConstruibles(UserIndex);
				WriteShowBlacksmithForm(UserIndex);
			} else {
				WriteConsoleMsg(UserIndex, "There's no anvil there.", FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, "There's no anvil there.", FontTypeNames_FONTTYPE_INFO);
		}
		break;

	default:
		break;
	}
}

/* '' */
/* ' Handles the "CreateNewGuild" message. */
/* ' */


void HoAClientPacketHandler::handleCreateNewGuild(CreateNewGuild* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/11/09 */
	/* '05/11/09: Pato - Ahora se quitan los espacios del principio y del fin del nombre del clan */
	/* '*************************************************** */

	std::string& desc = p->Desc;
	std::string GuildName;
	std::string& site = p->Site;
	std::vector<std::string> codex;
	std::string errorStr;

	GuildName = vb6::Trim(p->GuildName);
	codex = vb6::Split(p->Codex, SEPARATOR);
	
	if (CrearNuevoClan(UserIndex, desc, GuildName, site, codex, UserList[UserIndex].FundandoGuildAlineacion,
			errorStr)) {
		SendData(SendTarget_ToAll, UserIndex,
				hoa::protocol::server::BuildConsoleMsg(
						UserList[UserIndex].Name + " founded clan " + GuildName + " with alignment "
								+ GuildAlignment(UserList[UserIndex].GuildIndex) + ".",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildPlayWave(44, NO_3D_SOUND, NO_3D_SOUND));

		/* 'Update tag */
		RefreshCharStatus(UserIndex);
	} else {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "SpellInfo" message. */
/* ' */


void HoAClientPacketHandler::handleSpellInfo(SpellInfo* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int spellSlot;
	int Spell;

	spellSlot = p->Slot;

	/* 'Validate slot */
	if (spellSlot < 1 || spellSlot > MAXUSERHECHIZOS) {
		WriteConsoleMsg(UserIndex, "Pick a spell first!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate spell in the slot */
	Spell = UserList[UserIndex].Stats.UserHechizos[spellSlot];
	if (Spell > 0 && Spell < NumeroHechizos + 1) {
		/* 'Send information */
		WriteConsoleMsg(UserIndex,
				vb6::CStr("%%%%%%%%%%%% SPELL INFO %%%%%%%%%%%%") + vbCrLf + "Name:" + Hechizos[Spell].Nombre
						+ vbCrLf + "Description:" + Hechizos[Spell].desc + vbCrLf + "Required skill: "
						+ vb6::CStr(Hechizos[Spell].MinSkill) + " magic skill points." + vbCrLf + "Required mana: "
						+ vb6::CStr(Hechizos[Spell].ManaRequerido) + vbCrLf + "Required energy: "
						+ vb6::CStr(Hechizos[Spell].StaRequerido) + vbCrLf + "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "EquipItem" message. */
/* ' */


void HoAClientPacketHandler::handleEquipItem(EquipItem* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int itemSlot;

	itemSlot = p->Slot;

	/* 'Dead users can't equip items */
	if (UserList[UserIndex].flags.Muerto == 1) {
		return;
	}

	/* 'Validate item slot */
	if (itemSlot > UserList[UserIndex].CurrentInventorySlots || itemSlot < 1) {
		return;
	}

	if (UserList[UserIndex].Invent.Object[itemSlot].ObjIndex == 0) {
		return;
	}

	EquiparInvItem(UserIndex, itemSlot);
}

/* '' */
/* ' Handles the "ChangeHeading" message. */
/* ' */


void HoAClientPacketHandler::handleChangeHeading(ChangeHeading* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 06/28/2008 */
	/* 'Last Modified By: NicoNZ */
	/* ' 10/01/2008: Tavo - Se cancela la salida del juego si el user esta saliendo */
	/* ' 06/28/2008: NicoNZ - Sólo se puede cambiar si está inmovilizado. */
	/* '*************************************************** */

	eHeading heading;
	int posX = 0;
	int posY = 0;

	heading = static_cast<eHeading> (p->Heading);

	if (UserList[UserIndex].flags.Paralizado == 1 && UserList[UserIndex].flags.Inmovilizado == 0) {
		switch (heading) {
		case eHeading_NORTH:
			posY = -1;
			break;

		case eHeading_EAST:
			posX = 1;
			break;

		case eHeading_SOUTH:
			posY = 1;
			break;

		case eHeading_WEST:
			posX = -1;
			break;

		default:
			break;
		}

		if (LegalPos(UserList[UserIndex].Pos.Map, UserList[UserIndex].Pos.X + posX,
				UserList[UserIndex].Pos.Y + posY, vb6::CBool(UserList[UserIndex].flags.Navegando),
				!vb6::CBool(UserList[UserIndex].flags.Navegando))) {
			return;
		}
	}

	/* 'Validate heading (VB won't say invalid cast if not a valid index like .Net languages would do... *sigh*) */
	if (heading > 0 && heading < 5) {
		UserList[UserIndex].Char.heading = heading;
		ChangeUserChar(UserIndex, UserList[UserIndex].Char.body, UserList[UserIndex].Char.Head,
				UserList[UserIndex].Char.heading, UserList[UserIndex].Char.WeaponAnim,
				UserList[UserIndex].Char.ShieldAnim, UserList[UserIndex].Char.CascoAnim);
	}
}

/* '' */
/* ' Handles the "ModifySkills" message. */
/* ' */


void HoAClientPacketHandler::handleModifySkills(ModifySkills* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 11/19/09 */
	/* '11/19/09: Pato - Adapting to new skills system. */
	/* '*************************************************** */

	int i;
	int Count = 0;
	vb6::array<int> points;

	points.redim(1, NUMSKILLS);

	/* 'Codigo para prevenir el hackeo de los skills */
	/* '<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
	for (i = (1); i <= (NUMSKILLS); i++) {
		points[i] = p->Skills[i - 1];

		if (points[i] < 0) {
			LogHackAttemp(
					UserList[UserIndex].Name + " IP:" + UserList[UserIndex].ip
							+ " Tried to hack the skill system.");
			UserList[UserIndex].Stats.SkillPts = 0;
			/* FIXME: Ban IP */
			CloseSocket(UserIndex);
			return;
		}

		Count = Count + points[i];
	}

	if (Count > UserList[UserIndex].Stats.SkillPts) {
		LogHackAttemp(
				UserList[UserIndex].Name + " IP:" + UserList[UserIndex].ip + " Tried to hack the skill system.");
		/* FIXME: Ban IP */
		CloseSocket(UserIndex);
		return;
	}
	/* '<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

	UserList[UserIndex].Counters.AsignedSkills = MinimoInt(10,
			UserList[UserIndex].Counters.AsignedSkills + Count);

	for (i = (1); i <= (NUMSKILLS); i++) {
		if (points[i] > 0) {
			UserList[UserIndex].Stats.SkillPts = UserList[UserIndex].Stats.SkillPts - points[i];
			UserList[UserIndex].Stats.UserSkills[i] = UserList[UserIndex].Stats.UserSkills[i] + points[i];

			/* 'Client should prevent this, but just in case... */
			if (UserList[UserIndex].Stats.UserSkills[i] > 100) {
				/*UserList[UserIndex].Stats.SkillPts = UserList[UserIndex].Stats.SkillPts
						+ UserList[UserIndex].Stats.UserSkills[i] - 100;
				UserList[UserIndex].Stats.UserSkills[i] = 100;*/
				UserList[UserIndex].Stats.UserSkills[i] = 0;
				LogHackAttemp(
						UserList[UserIndex].Name + " IP:" + UserList[UserIndex].ip
								+ " Tried to hack the skill system.");
				UserList[UserIndex].Stats.SkillPts = 0;
				/* FIXME: Ban IP */
				CloseSocket(UserIndex);
				return;
			}

			CheckEluSkill(UserIndex, i, true);
		}
	}
}

/* '' */
/* ' Handles the "Train" message. */
/* ' */


void HoAClientPacketHandler::handleTrain(Train* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int SpawnedNpc;
	int PetIndex;

	PetIndex = p->PetIndex;

	if (UserList[UserIndex].flags.TargetNPC == 0) {
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Entrenador) {
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].Mascotas < MAXMASCOTASENTRENADOR) {
		if (PetIndex > 0 && PetIndex < Npclist[UserList[UserIndex].flags.TargetNPC].NroCriaturas + 1) {
			/* 'Create the creature */
			SpawnedNpc = SpawnNpc(Npclist[UserList[UserIndex].flags.TargetNPC].Criaturas[PetIndex].NpcIndex,
					Npclist[UserList[UserIndex].flags.TargetNPC].Pos, true, false);

			if (SpawnedNpc > 0) {
				Npclist[SpawnedNpc].MaestroNpc = UserList[UserIndex].flags.TargetNPC;
				Npclist[UserList[UserIndex].flags.TargetNPC].Mascotas =
						Npclist[UserList[UserIndex].flags.TargetNPC].Mascotas + 1;
			}
		}
	} else {
		SendData(SendTarget_ToPCArea, UserIndex,
				BuildChatOverHead("No puedo traer más criaturas, mata las existentes.",
						Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff));
	}
}

/* '' */
/* ' Handles the "CommerceBuy" message. */
/* ' */


void HoAClientPacketHandler::handleCommerceBuy(CommerceBuy* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Slot;
	int Amount;

	Slot = p->Slot;
	Amount = p->Amount;

	/* 'Dead people can't commerce... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* '¿El target es un NPC valido? */
	if (UserList[UserIndex].flags.TargetNPC < 1) {
		return;
	}

	/* '¿El NPC puede comerciar? */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].Comercia == 0) {
		SendData(SendTarget_ToPCArea, UserIndex,
				BuildChatOverHead("I've no interest in trading.",
						Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff));
		return;
	}

	/* 'Only if in commerce mode.... */
	if (!UserList[UserIndex].flags.Comerciando) {
		WriteConsoleMsg(UserIndex, "You are not trading.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'User compra el item */
	Comercio(eModoComercio_Compra, UserIndex, UserList[UserIndex].flags.TargetNPC, Slot, Amount);
}

/* '' */
/* ' Handles the "BankExtractItem" message. */
/* ' */


void HoAClientPacketHandler::handleBankExtractItem(BankExtractItem* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Slot;
	int Amount;

	Slot = p->Slot;
	Amount = p->Amount;

	/* 'Dead people can't commerce */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* '¿El target es un NPC valido? */
	if (UserList[UserIndex].flags.TargetNPC < 1) {
		return;
	}

	/* '¿Es el banquero? */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Banquero) {
		return;
	}

	/* 'User retira el item del slot */
	UserRetiraItem(UserIndex, Slot, Amount);
}

/* '' */
/* ' Handles the "CommerceSell" message. */
/* ' */


void HoAClientPacketHandler::handleCommerceSell(CommerceSell* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Slot;
	int Amount;

	Slot = p->Slot;
	Amount = p->Amount;

	/* 'Dead people can't commerce... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* '¿El target es un NPC valido? */
	if (UserList[UserIndex].flags.TargetNPC < 1) {
		return;
	}

	/* '¿El NPC puede comerciar? */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].Comercia == 0) {
		SendData(SendTarget_ToPCArea, UserIndex,
				BuildChatOverHead("I've no interest in trading",
						Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff));
		return;
	}

	/* 'User compra el item del slot */
	Comercio(eModoComercio_Venta, UserIndex, UserList[UserIndex].flags.TargetNPC, Slot, Amount);
}

/* '' */
/* ' Handles the "BankDeposit" message. */
/* ' */


void HoAClientPacketHandler::handleBankDeposit(BankDeposit* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Slot;
	int Amount;

	Slot = p->Slot;
	Amount = p->Amount;

	/* 'Dead people can't commerce... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* '¿El target es un NPC valido? */
	if (UserList[UserIndex].flags.TargetNPC < 1) {
		return;
	}

	/* '¿El NPC puede comerciar? */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Banquero) {
		return;
	}

	/* 'User deposita el item del slot rdata */
	UserDepositaItem(UserIndex, Slot, Amount);
}

/* '' */
/* ' Handles the "ForumPost" message. */
/* ' */


void HoAClientPacketHandler::handleForumPost(ForumPost* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 02/01/2010 */
	/* '02/01/2010: ZaMa - Implemento nuevo sistema de foros */
	/* '*************************************************** */

	eForumMsgType ForumMsgType;

	std::string File;
	std::string Title;
	std::string Post;
	int ForumIndex = 0;
	std::string postFile;
	int ForumType;

	ForumMsgType = static_cast<eForumMsgType>(p->MsgType);

	Title = p->Title;
	Post = p->Post;

	if (UserList[UserIndex].flags.TargetObj > 0) {
		ForumType = ForumAlignment(ForumMsgType);

		switch (ForumType) {

		case eForumType_ieGeneral:
			ForumIndex = GetForumIndex(ObjData[UserList[UserIndex].flags.TargetObj].ForoID);

			break;

		case eForumType_ieREAL:
			ForumIndex = GetForumIndex(FORO_REAL_ID);

			break;

		case eForumType_ieCAOS:
			ForumIndex = GetForumIndex(FORO_CAOS_ID);

			break;
		}

		AddPost(ForumIndex, Post, UserList[UserIndex].Name, Title, EsAnuncio(ForumMsgType));
	}
}

/* '' */
/* ' Handles the "MoveSpell" message. */
/* ' */


void HoAClientPacketHandler::handleMoveSpell(MoveSpell* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int dir;

	if (p->Direction) {
		dir = 1;
	} else {
		dir = -1;
	}

	DesplazarHechizo(UserIndex, dir, p->Slot);
}

/* '' */
/* ' Handles the "MoveBank" message. */
/* ' */


void HoAClientPacketHandler::handleMoveBank(MoveBank* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Torres Patricio (Pato) */
	/* 'Last Modification: 06/14/09 */
	/* ' */
	/* '*************************************************** */

	int dir;
	int Slot;
	struct Obj TempItem;

	if (p->Direction) {
		dir = 1;
	} else {
		dir = -1;
	}

	Slot = p->Slot;

	TempItem.ObjIndex = UserList[UserIndex].BancoInvent.Object[Slot].ObjIndex;
	TempItem.Amount = UserList[UserIndex].BancoInvent.Object[Slot].Amount;

	/* 'Mover arriba */
	if (dir == 1) {
		UserList[UserIndex].BancoInvent.Object[Slot] = UserList[UserIndex].BancoInvent.Object[Slot - 1];
		UserList[UserIndex].BancoInvent.Object[Slot - 1].ObjIndex = TempItem.ObjIndex;
		UserList[UserIndex].BancoInvent.Object[Slot - 1].Amount = TempItem.Amount;
		/* 'mover abajo */
	} else {
		UserList[UserIndex].BancoInvent.Object[Slot] = UserList[UserIndex].BancoInvent.Object[Slot + 1];
		UserList[UserIndex].BancoInvent.Object[Slot + 1].ObjIndex = TempItem.ObjIndex;
		UserList[UserIndex].BancoInvent.Object[Slot + 1].Amount = TempItem.Amount;
	}

	UpdateBanUserInv(true, UserIndex, 0);
	UpdateVentanaBanco(UserIndex);

}

/* '' */
/* ' Handles the "ClanCodexUpdate" message. */
/* ' */


void HoAClientPacketHandler::handleClanCodexUpdate(ClanCodexUpdate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string url;
	std::string desc;
	std::vector<std::string> codex;

	desc = p->Desc;
	codex = vb6::Split(p->Codex, SEPARATOR);
	url = p->Url;

	ChangeCodexAndDesc(desc, codex, url, UserList[UserIndex].GuildIndex);
}

/* '' */
/* ' Handles the "UserCommerceOffer" message. */
/* ' */


void HoAClientPacketHandler::handleUserCommerceOffer(UserCommerceOffer* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 24/11/2009 */
	/* '24/11/2009: ZaMa - Nuevo sistema de comercio */
	/* '*************************************************** */

	int Amount;
	int Slot;
	int tUser = 0;
	int OfferSlot;
	int ObjIndex = 0;

	Slot = p->Slot;
	Amount = p->Amount;
	OfferSlot = p->OfferSlot;

	if (!PuedeSeguirComerciando(UserIndex)) {
		return;
	}

	/* 'Get the other player */
	tUser = UserList[UserIndex].ComUsu.DestUsu;

	/* ' If he's already confirmed his offer, but now tries to change it, then he's cheating */
	if (UserList[UserIndex].ComUsu.Confirmo == true) {

		/* ' Finish the trade */
		FinComerciarUsu(UserIndex);
		FinComerciarUsu(tUser);
		FlushBuffer(tUser);

		return;
	}

	/* 'If slot is invalid and it's not gold or it's not 0 (Substracting), then ignore it. */
	if (((Slot < 0 || Slot > UserList[UserIndex].CurrentInventorySlots) && Slot != FLAGORO)) {
		return;
	}

	/* 'If OfferSlot is invalid, then ignore it. */
	if (OfferSlot < 1 || OfferSlot > MAX_OFFER_SLOTS + 1) {
		return;
	}

	/* ' Can be negative if substracted from the offer, but never 0. */
	if (Amount == 0) {
		return;
	}

	/* 'Has he got enough?? */
	if (Slot == FLAGORO) {
		/* ' Can't offer more than he has */
		if (Amount > UserList[UserIndex].Stats.GLD - UserList[UserIndex].ComUsu.GoldAmount) {
			WriteCommerceChat(UserIndex, "You don't have that amount of gold to add to your offer.",
					FontTypeNames_FONTTYPE_TALK);
			return;
		}

		if (Amount < 0) {
			if (vb6::Abs(Amount) > UserList[UserIndex].ComUsu.GoldAmount) {
				Amount = UserList[UserIndex].ComUsu.GoldAmount * (-1);
			}
		}
	} else {
		/* 'If modifing a filled offerSlot, we already got the objIndex, then we don't need to know it */
		if (Slot != 0) {
			ObjIndex = UserList[UserIndex].Invent.Object[Slot].ObjIndex;
		}

		/* ' Non-Transferible or commerciable? */
		if (ObjIndex != 0) {
			if ((ObjData[ObjIndex].Intransferible == 1 || ObjData[ObjIndex].NoComerciable == 1)) {
				WriteCommerceChat(UserIndex, "You can't trade that item.", FontTypeNames_FONTTYPE_TALK);
				return;
			}
		}

		/* ' Can't offer more than he has */
		if (!HasEnoughItems(UserIndex, ObjIndex, TotalOfferItems(ObjIndex, UserIndex) + Amount)) {

			WriteCommerceChat(UserIndex, "You don't have that amount.", FontTypeNames_FONTTYPE_TALK);
			return;
		}

		if (Amount < 0) {
			if (vb6::Abs(Amount) > UserList[UserIndex].ComUsu.cant[OfferSlot]) {
				Amount = UserList[UserIndex].ComUsu.cant[OfferSlot] * (-1);
			}
		}

		if (ItemNewbie(ObjIndex)) {
			WriteCancelOfferItem(UserIndex, OfferSlot);
			return;
		}

		/* 'Don't allow to sell boats if they are equipped (you can't take them off in the water and causes trouble) */
		if (UserList[UserIndex].flags.Navegando == 1) {
			if (UserList[UserIndex].Invent.BarcoSlot == Slot) {
				WriteCommerceChat(UserIndex, "You can't sell your boat while you're using it.",
						FontTypeNames_FONTTYPE_TALK);
				return;
			}
		}

		if (UserList[UserIndex].Invent.MochilaEqpSlot > 0) {
			if (UserList[UserIndex].Invent.MochilaEqpSlot == Slot) {
				WriteCommerceChat(UserIndex, "You can't sell your backpack while you're using it.",
						FontTypeNames_FONTTYPE_TALK);
				return;
			}
		}
	}

	AgregarOferta(UserIndex, OfferSlot, ObjIndex, Amount, Slot == FLAGORO);
	EnviarOferta(tUser, OfferSlot);

}

/* '' */
/* ' Handles the "GuildAcceptPeace" message. */
/* ' */


void HoAClientPacketHandler::handleGuildAcceptPeace(GuildAcceptPeace* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	int otherClanIndex;

	guild = p->Guild;

	otherClanIndex = r_AceptarPropuestaDePaz(UserIndex, guild, errorStr);

	if (otherClanIndex == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg("Your clan has made peace with " + guild + ".",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, otherClanIndex,
				hoa::protocol::server::BuildConsoleMsg(
						"Your clan has made peace with " + GuildName(UserList[UserIndex].GuildIndex) + ".",
						FontTypeNames_FONTTYPE_GUILD));
	}



}

/* '' */
/* ' Handles the "GuildRejectAlliance" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRejectAlliance(GuildRejectAlliance* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	int otherClanIndex;

	guild = p->Guild;

	otherClanIndex = r_RechazarPropuestaDeAlianza(UserIndex, guild, errorStr);

	if (otherClanIndex == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg("Your clan has rejected an alliance with " + guild,
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, otherClanIndex,
				hoa::protocol::server::BuildConsoleMsg(
						GuildName(UserList[UserIndex].GuildIndex)
								+ " has rejected our alliance proposal.",
						FontTypeNames_FONTTYPE_GUILD));
	}




}

/* '' */
/* ' Handles the "GuildRejectPeace" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRejectPeace(GuildRejectPeace* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	int otherClanIndex;

	guild = p->Guild;

	otherClanIndex = r_RechazarPropuestaDePaz(UserIndex, guild, errorStr);

	if (otherClanIndex == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg("Your clan has rejected the peace proposal sent by " + guild + ".",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, otherClanIndex,
				hoa::protocol::server::BuildConsoleMsg(
						GuildName(UserList[UserIndex].GuildIndex)
								+ " has rejected our peace proposal.",
						FontTypeNames_FONTTYPE_GUILD));
	}




}

/* '' */
/* ' Handles the "GuildAcceptAlliance" message. */
/* ' */


void HoAClientPacketHandler::handleGuildAcceptAlliance(GuildAcceptAlliance* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	int otherClanIndex;

	guild = p->Guild;

	otherClanIndex = r_AceptarPropuestaDeAlianza(UserIndex, guild, errorStr);

	if (otherClanIndex == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg("Your clan has entered into an alliance with " + guild + ".",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, otherClanIndex,
				hoa::protocol::server::BuildConsoleMsg(
						"Your clan has signed a peace agreement with " + GuildName(UserList[UserIndex].GuildIndex) + ".",
						FontTypeNames_FONTTYPE_GUILD));
	}
}

/* '' */
/* ' Handles the "GuildOfferPeace" message. */
/* ' */


void HoAClientPacketHandler::handleGuildOfferPeace(GuildOfferPeace* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string proposal;
	std::string errorStr;

	guild = p->Guild;
	proposal = p->Proposal;

	if (r_ClanGeneraPropuesta(UserIndex, guild, RELACIONES_GUILD_PAZ, proposal, errorStr)) {
		WriteConsoleMsg(UserIndex, "Peace proposal sent.", FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "GuildOfferAlliance" message. */
/* ' */


void HoAClientPacketHandler::handleGuildOfferAlliance(GuildOfferAlliance* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string proposal;
	std::string errorStr;

	guild = p->Guild;
	proposal = p->Proposal;

	if (r_ClanGeneraPropuesta(UserIndex, guild, RELACIONES_GUILD_ALIADOS, proposal, errorStr)) {
		WriteConsoleMsg(UserIndex, "Alliance proposal sent.", FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "GuildAllianceDetails" message. */
/* ' */


void HoAClientPacketHandler::handleGuildAllianceDetails(GuildAllianceDetails* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	std::string details;

	guild = p->Guild;

	details = r_VerPropuesta(UserIndex, guild, RELACIONES_GUILD_ALIADOS, errorStr);

	if (vb6::LenB(details) == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteOfferDetails(UserIndex, details);
	}
}

/* '' */
/* ' Handles the "GuildPeaceDetails" message. */
/* ' */


void HoAClientPacketHandler::handleGuildPeaceDetails(GuildPeaceDetails* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	std::string details;

	guild = p->Guild;

	details = r_VerPropuesta(UserIndex, guild, RELACIONES_GUILD_PAZ, errorStr);

	if (vb6::LenB(details) == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteOfferDetails(UserIndex, details);
	}
}

/* '' */
/* ' Handles the "GuildRequestJoinerInfo" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRequestJoinerInfo(GuildRequestJoinerInfo* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string User;
	std::string details;

	User = p->User;

	details = a_DetallesAspirante(UserIndex, User);

	if (vb6::LenB(details) == 0) {
		WriteConsoleMsg(UserIndex, "The PC hasn't send an application, or you don't have permission to see it.",
				FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteShowUserRequest(UserIndex, details);
	}
}

/* '' */
/* ' Handles the "GuildAlliancePropList" message. */
/* ' */


void HoAClientPacketHandler::handleGuildAlliancePropList(GuildAlliancePropList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	WriteAlianceProposalsList(UserIndex, r_ListaDePropuestas(UserIndex, RELACIONES_GUILD_ALIADOS));
}

/* '' */
/* ' Handles the "GuildPeacePropList" message. */
/* ' */


void HoAClientPacketHandler::handleGuildPeacePropList(GuildPeacePropList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	WritePeaceProposalsList(UserIndex, r_ListaDePropuestas(UserIndex, RELACIONES_GUILD_PAZ));
}

/* '' */
/* ' Handles the "GuildDeclareWar" message. */
/* ' */


void HoAClientPacketHandler::handleGuildDeclareWar(GuildDeclareWar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string errorStr;
	int otherGuildIndex;

	guild = p->Guild;

	otherGuildIndex = r_DeclararGuerra(UserIndex, guild, errorStr);

	if (otherGuildIndex == 0) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		/* 'WAR shall be! */
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg("YOUR CLAN IS NOW IN WAR WITH " + guild + ".",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, otherGuildIndex,
				hoa::protocol::server::BuildConsoleMsg(
						GuildName(UserList[UserIndex].GuildIndex) + " DECLARES WAR TO YOUR CLAN.",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildPlayWave(45, NO_3D_SOUND, NO_3D_SOUND));
		SendData(SendTarget_ToGuildMembers, otherGuildIndex,
				hoa::protocol::server::BuildPlayWave(45, NO_3D_SOUND, NO_3D_SOUND));
	}




}

/* '' */
/* ' Handles the "GuildNewWebsite" message. */
/* ' */


void HoAClientPacketHandler::handleGuildNewWebsite(GuildNewWebsite* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	ActualizarWebSite(UserIndex, p->Website);
}

/* '' */
/* ' Handles the "GuildAcceptNewMember" message. */
/* ' */


void HoAClientPacketHandler::handleGuildAcceptNewMember(GuildAcceptNewMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string errorStr;
	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!a_AceptarAspirante(UserIndex, UserName, errorStr)) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		tUser = NameIndex(UserName);
		if (tUser > 0) {
			m_ConectarMiembroAClan(tUser, UserList[UserIndex].GuildIndex);
			RefreshCharStatus(tUser);
		}

		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg(UserName + " has been accepted as a member of your clan.",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildPlayWave(43, NO_3D_SOUND, NO_3D_SOUND));
	}




}

/* '' */
/* ' Handles the "GuildRejectNewMember" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRejectNewMember(GuildRejectNewMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/08/07 */
	/* 'Last Modification by: (liquid) */
	/* ' */
	/* '*************************************************** */

	std::string errorStr;
	std::string UserName;
	std::string Reason;
	int tUser = 0;

	UserName = p->UserName;
	Reason = p->Reason;

	if (!a_RechazarAspirante(UserIndex, UserName, errorStr)) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		tUser = NameIndex(UserName);

		if (tUser > 0) {
			WriteConsoleMsg(tUser, errorStr + " : " + Reason, FontTypeNames_FONTTYPE_GUILD);
		} else {
			/* 'hay que grabar en el char su rechazo */
			a_RechazarAspiranteChar(UserName, UserList[UserIndex].GuildIndex, Reason);
		}
	}




}

/* '' */
/* ' Handles the "GuildKickMember" message. */
/* ' */


void HoAClientPacketHandler::handleGuildKickMember(GuildKickMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int GuildIndex;

	UserName = p->UserName;

	GuildIndex = m_EcharMiembroDeClan(UserIndex, UserName);

	if (GuildIndex > 0) {
		SendData(SendTarget_ToGuildMembers, GuildIndex,
				hoa::protocol::server::BuildConsoleMsg(UserName + " was expelled from the clan.",
						FontTypeNames_FONTTYPE_GUILD));
		SendData(SendTarget_ToGuildMembers, GuildIndex, hoa::protocol::server::BuildPlayWave(45, NO_3D_SOUND, NO_3D_SOUND));
	} else {
		WriteConsoleMsg(UserIndex, "You cannot expel this PC from the clan.",
				FontTypeNames_FONTTYPE_GUILD);
	}

}

/* '' */
/* ' Handles the "GuildUpdateNews" message. */
/* ' */


void HoAClientPacketHandler::handleGuildUpdateNews(GuildUpdateNews* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	ActualizarNoticias(UserIndex, p->News);
}

/* '' */
/* ' Handles the "GuildMemberInfo" message. */
/* ' */


void HoAClientPacketHandler::handleGuildMemberInfo(GuildMemberInfo* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	SendDetallesPersonaje(UserIndex, p->UserName);
}

/* '' */
/* ' Handles the "GuildOpenElections" message. */
/* ' */


void HoAClientPacketHandler::handleGuildOpenElections(GuildOpenElections* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string ERROR;

	if (!v_AbrirElecciones(UserIndex, ERROR)) {
		WriteConsoleMsg(UserIndex, ERROR, FontTypeNames_FONTTYPE_GUILD);
	} else {
		SendData(SendTarget_ToGuildMembers, UserList[UserIndex].GuildIndex,
				hoa::protocol::server::BuildConsoleMsg(
						"Clan elections have started! You can vote by typing /VOTE followed by the candidate you wish to vote for "
								+ UserList[UserIndex].Name, FontTypeNames_FONTTYPE_GUILD));
	}
}

/* '' */
/* ' Handles the "GuildRequestMembership" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRequestMembership(GuildRequestMembership* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	std::string application;
	std::string errorStr;

	guild = p->Guild;
	application = p->Application;

	if (!a_NuevoAspirante(UserIndex, guild, application, errorStr)) {
		WriteConsoleMsg(UserIndex, errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteConsoleMsg(UserIndex,
				"Your request has been sent. Wait for news from the leader of " + guild + ".",
				FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "GuildRequestDetails" message. */
/* ' */


void HoAClientPacketHandler::handleGuildRequestDetails(GuildRequestDetails* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	SendGuildDetails(UserIndex, p->Guild);

}

/* '' */
/* ' Handles the "Online" message. */
/* ' */


void HoAClientPacketHandler::handleOnline(Online* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int i;
	int Count = 0;

	for (i = (1); i <= (LastUser); i++) {
		if (vb6::LenB(UserList[i].Name) != 0) {
			if (UserTieneAlgunPrivilegios(i, PlayerType_User, PlayerType_Consejero)) {
				Count = Count + 1;
			}
		}
	}

	WriteConsoleMsg(UserIndex, "Number of users: " + vb6::CStr(Count), FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "Quit" message. */
/* ' */


void HoAClientPacketHandler::handleQuit(Quit* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 04/15/2008 (NicoNZ) */
	/* 'If user is invisible, it automatically becomes */
	/* 'visible before doing the countdown to exit */
	/* '04/15/2008 - No se reseteaban lso contadores de invi ni de ocultar. (NicoNZ) */
	/* '*************************************************** */

	if (UserList[UserIndex].flags.Paralizado == 1) {
		WriteConsoleMsg(UserIndex, "You can't leave the game while you're paralyzed.", FontTypeNames_FONTTYPE_WARNING);
		return;
	}

	CerrarUserIndexIniciar(UserIndex);
}

/* '' */
/* ' Handles the "GuildLeave" message. */
/* ' */


void HoAClientPacketHandler::handleGuildLeave(GuildLeave* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int GuildIndex;

	/* 'obtengo el guildindex */
	GuildIndex = m_EcharMiembroDeClan(UserIndex, UserList[UserIndex].Name);

	if (GuildIndex > 0) {
		WriteConsoleMsg(UserIndex, "You leave the clan.", FontTypeNames_FONTTYPE_GUILD);
		SendData(SendTarget_ToGuildMembers, GuildIndex,
				hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + " deja el clan.",
						FontTypeNames_FONTTYPE_GUILD));
	} else {
		WriteConsoleMsg(UserIndex, "You cannot leave this clan.", FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "RequestAccountState" message. */
/* ' */


void HoAClientPacketHandler::handleRequestAccountState(RequestAccountState* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int earnings = 0;
	int Percentage = 0;




	/* 'Dead people can't check their accounts */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 3) {
		WriteConsoleMsg(UserIndex, "You're too far away from the seller.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	switch (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype) {
	case eNPCType_Banquero:
		WriteChatOverHead(UserIndex,
				"You have " + vb6::CStr(UserList[UserIndex].Stats.Banco) + " gp in your account.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

		break;

	case eNPCType_Timbero:
		if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
			earnings = Apuestas.Ganancias - Apuestas.Perdidas;

			if (earnings >= 0 && Apuestas.Ganancias != 0) {
				Percentage = vb6::Int(earnings * 100 / Apuestas.Ganancias);
			}

			if (earnings < 0 && Apuestas.Perdidas != 0) {
				Percentage = vb6::Int(earnings * 100 / Apuestas.Perdidas);
			}

			WriteConsoleMsg(UserIndex,
					"Winnings: " + vb6::CStr(Apuestas.Ganancias) + " Losses: " + vb6::CStr(Apuestas.Perdidas) + " Net Winnings: "
							+ vb6::CStr(earnings) + " (" + vb6::CStr(Percentage) + "%) Bets: " + vb6::CStr(Apuestas.Jugadas),
					FontTypeNames_FONTTYPE_INFO);
		}
		break;

	default:
		break;
	}
}

/* '' */
/* ' Handles the "PetStand" message. */
/* ' */


void HoAClientPacketHandler::handlePetStand(PetStand* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead people can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make sure it's close enough */
	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make sure it's his pet */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].MaestroUser != UserIndex) {
		return;
	}

	/* 'Do it! */
	Npclist[UserList[UserIndex].flags.TargetNPC].Movement = TipoAI_ESTATICO;

	Expresar(UserList[UserIndex].flags.TargetNPC, UserIndex);
}

/* '' */
/* ' Handles the "PetFollow" message. */
/* ' */


void HoAClientPacketHandler::handlePetFollow(PetFollow* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead users can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make sure it's close enough */
	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make usre it's the user's pet */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].MaestroUser != UserIndex) {
		return;
	}

	/* 'Do it */
	FollowAmo(UserList[UserIndex].flags.TargetNPC);

	Expresar(UserList[UserIndex].flags.TargetNPC, UserIndex);
}

/* '' */
/* ' Handles the "ReleasePet" message. */
/* ' */


void HoAClientPacketHandler::handleReleasePet(ReleasePet* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 18/11/2009 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead users can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"You must first left-click a pet.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make usre it's the user's pet */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].MaestroUser != UserIndex) {
		return;
	}

	/* 'Make sure it's close enough */
	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Do it */
	QuitarPet(UserIndex, UserList[UserIndex].flags.TargetNPC);

}

/* '' */
/* ' Handles the "TrainList" message. */
/* ' */


void HoAClientPacketHandler::handleTrainList(TrainList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead users can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make sure it's close enough */
	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Make sure it's the trainer */
	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Entrenador) {
		return;
	}

	WriteTrainerCreatureList(UserIndex, UserList[UserIndex].flags.TargetNPC);
}

/* '' */
/* ' Handles the "Rest" message. */
/* ' */


void HoAClientPacketHandler::handleRest(Rest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead users can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!! Solo puedes usar ítems cuando estás vivo.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (HayOBJarea(UserList[UserIndex].Pos, FOGATA)) {
		WriteRestOK(UserIndex);

		if (!UserList[UserIndex].flags.Descansar) {
			WriteConsoleMsg(UserIndex, "You sit next to the fire and start resting.",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteConsoleMsg(UserIndex, "You stand up.", FontTypeNames_FONTTYPE_INFO);
		}

		UserList[UserIndex].flags.Descansar = !UserList[UserIndex].flags.Descansar;
	} else {
		if (UserList[UserIndex].flags.Descansar) {
			WriteRestOK(UserIndex);
			WriteConsoleMsg(UserIndex, "You stand up.", FontTypeNames_FONTTYPE_INFO);

			UserList[UserIndex].flags.Descansar = false;
			return;
		}

		WriteConsoleMsg(UserIndex, "There's no fire to rest by.",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "Meditate" message. */
/* ' */


void HoAClientPacketHandler::handleMeditate(Meditate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 04/15/08 (NicoNZ) */
	/* 'Arreglé un bug que mandaba un index de la meditacion diferente */
	/* 'al que decia el server. */
	/* '*************************************************** */



	/* 'Dead users can't use pets */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!! You can only meditate while you're alive.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Can he meditate? */
	if (UserList[UserIndex].Stats.MaxMAN == 0) {
		WriteConsoleMsg(UserIndex, "Only the magical classes know the art of meditation.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Admins don't have to wait :D */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		UserList[UserIndex].Stats.MinMAN = UserList[UserIndex].Stats.MaxMAN;
		WriteConsoleMsg(UserIndex, "Mana restored.", FontTypeNames_FONTTYPE_VENENO);
		WriteUpdateMana(UserIndex);
		return;
	}

	WriteMeditateToggle(UserIndex);

	if (UserList[UserIndex].flags.Meditando) {
		WriteConsoleMsg(UserIndex, "You stop meditating.", FontTypeNames_FONTTYPE_INFO);
	}

	UserList[UserIndex].flags.Meditando = !UserList[UserIndex].flags.Meditando;

	/* 'Barrin 3/10/03 Tiempo de inicio al meditar */
	if (UserList[UserIndex].flags.Meditando) {
		UserList[UserIndex].Counters.tInicioMeditar = vb6::GetTickCount();

		/* 'Call WriteConsoleMsg(UserIndex, "Te estás concentrando. En " & Fix(TIEMPO_INICIOMEDITAR / 1000) & " segundos comenzarás a meditar.", FontTypeNames.FONTTYPE_INFO) */
		/* ' [TEMPORAL] */
		int num_seconds = UserList[UserIndex].Stats.ELV / 17;
		std::string seconds = vb6::CStr(num_seconds);
		std::string word_seconds = " second";
		if (num_seconds > 1) {
			word_seconds += "s";
		}

		WriteConsoleMsg(UserIndex,
				"You're concentrating. In " + seconds
						+ word_seconds + ", you'll start meditating.", FontTypeNames_FONTTYPE_INFO);

		UserList[UserIndex].Char.loops = INFINITE_LOOPS;

		/* 'Show proper FX according to level */
		if (UserList[UserIndex].Stats.ELV < 13) {
			UserList[UserIndex].Char.FX = FXIDs_FXMEDITARCHICO;

		} else if (UserList[UserIndex].Stats.ELV < 25) {
			UserList[UserIndex].Char.FX = FXIDs_FXMEDITARMEDIANO;

		} else if (UserList[UserIndex].Stats.ELV < 35) {
			UserList[UserIndex].Char.FX = FXIDs_FXMEDITARGRANDE;

		} else if (UserList[UserIndex].Stats.ELV < 42) {
			UserList[UserIndex].Char.FX = FXIDs_FXMEDITARXGRANDE;

		} else {
			UserList[UserIndex].Char.FX = FXIDs_FXMEDITARXXGRANDE;
		}

		SendData(SendTarget_ToPCArea, UserIndex,
				hoa::protocol::server::BuildCreateFX(UserList[UserIndex].Char.CharIndex, UserList[UserIndex].Char.FX,
						INFINITE_LOOPS));
	} else {
		UserList[UserIndex].Counters.bPuedeMeditar = false;

		UserList[UserIndex].Char.FX = 0;
		UserList[UserIndex].Char.loops = 0;
		SendData(SendTarget_ToPCArea, UserIndex,
				hoa::protocol::server::BuildCreateFX(UserList[UserIndex].Char.CharIndex, 0, 0));
	}
}

/* '' */
/* ' Handles the "Resucitate" message. */
/* ' */


void HoAClientPacketHandler::handleResucitate(Resucitate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Se asegura que el target es un npc */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate NPC and make sure player is dead */
	if ((Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Revividor
			&& (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_ResucitadorNewbie
					|| !EsNewbie(UserIndex))) || UserList[UserIndex].flags.Muerto == 0) {
		return;
	}

	/* 'Make sure it's close enough */
	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "The priest cannot resucitate you because your too far away.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	RevivirUsuario(UserIndex);
	WriteConsoleMsg(UserIndex, "You've been raised from the dead!!", FontTypeNames_FONTTYPE_INFO);

}

/* '' */
/* ' Handles the "Consultation" message. */
/* ' */


void HoAClientPacketHandler::handleConsultation(Consultation* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 01/05/2010 */
	/* 'Habilita/Deshabilita el modo consulta. */
	/* '01/05/2010: ZaMa - Agrego validaciones. */
	/* '16/09/2010: ZaMa - No se hace visible en los clientes si estaba navegando (porque ya lo estaba). */
	/* '*************************************************** */

	int UserConsulta;




	/* ' Comando exclusivo para gms */
	if (!EsGm(UserIndex)) {
		return;
	}

	UserConsulta = UserList[UserIndex].flags.TargetUser;

	/* 'Se asegura que el target es un usuario */
	if (UserConsulta == 0) {
		WriteConsoleMsg(UserIndex, "First, left-click a user.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* ' No podes ponerte a vos mismo en modo consulta. */
	if (UserConsulta == UserIndex) {
		return;
	}

	/* ' No podes estra en consulta con otro gm */
	if (EsGm(UserConsulta)) {
		WriteConsoleMsg(UserIndex, "You cannot enter support mode with a second admin.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	std::string UserName;
	UserName = UserList[UserConsulta].Name;

	/* ' Si ya estaba en consulta, termina la consulta */
	if (UserList[UserConsulta].flags.EnConsulta) {
		WriteConsoleMsg(UserIndex, "You've ended your support session with " + UserName + ".",
				FontTypeNames_FONTTYPE_INFOBOLD);
		WriteConsoleMsg(UserConsulta, "You've ended your support session.", FontTypeNames_FONTTYPE_INFOBOLD);
		LogGM(UserList[UserIndex].Name, "Ended their support session with " + UserName);

		UserList[UserConsulta].flags.EnConsulta = false;

		/* ' Sino la inicia */
	} else {
		WriteConsoleMsg(UserIndex, "You've started a support session with " + UserName + ".",
				FontTypeNames_FONTTYPE_INFOBOLD);
		WriteConsoleMsg(UserConsulta, "You've started a support session.", FontTypeNames_FONTTYPE_INFOBOLD);
		LogGM(UserList[UserIndex].Name, "Started a support session with " + UserName);

		UserList[UserConsulta].flags.EnConsulta = true;

		/* ' Pierde invi u ocu */
		if (UserList[UserConsulta].flags.invisible == 1 || UserList[UserConsulta].flags.Oculto == 1) {
			UserList[UserConsulta].flags.Oculto = 0;
			UserList[UserConsulta].flags.invisible = 0;
			UserList[UserConsulta].Counters.TiempoOculto = 0;
			UserList[UserConsulta].Counters.Invisibilidad = 0;

			if (UserList[UserConsulta].flags.Navegando == 0) {
				SetInvisible(UserConsulta, UserList[UserConsulta].Char.CharIndex, false);
			}
		}
	}

	SetConsulatMode(UserConsulta);

}

/* '' */
/* ' Handles the "Heal" message. */
/* ' */


void HoAClientPacketHandler::handleHeal(Heal* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Se asegura que el target es un npc */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if ((Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Revividor
			&& Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_ResucitadorNewbie)
			|| UserList[UserIndex].flags.Muerto != 0) {
		return;
	}

	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "The priest cannot cure you because you are too far away.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	UserList[UserIndex].Stats.MinHp = UserList[UserIndex].Stats.MaxHp;

	WriteUpdateHP(UserIndex);

	WriteConsoleMsg(UserIndex, "You've been cured!!", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "RequestStats" message. */
/* ' */


void HoAClientPacketHandler::handleRequestStats(RequestStats* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	SendUserStatsTxt(UserIndex, UserIndex);
}

/* '' */
/* ' Handles the "Help" message. */
/* ' */


void HoAClientPacketHandler::handleHelp(Help* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	SendHelp(UserIndex);
}

/* '' */
/* ' Handles the "CommerceStart" message. */
/* ' */


void HoAClientPacketHandler::handleCommerceStart(CommerceStart* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int i;



	/* 'Dead people can't commerce */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Is it already in commerce mode?? */
	if (UserList[UserIndex].flags.Comerciando) {
		WriteConsoleMsg(UserIndex, "You're already trading.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC > 0) {
		/* 'Does the NPC want to trade?? */
		if (Npclist[UserList[UserIndex].flags.TargetNPC].Comercia == 0) {
			if (vb6::LenB(Npclist[UserList[UserIndex].flags.TargetNPC].desc) != 0) {
				WriteChatOverHead(UserIndex, "I've no interest in trading",
						Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
			}

			return;
		}

		if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 3) {
			WriteConsoleMsg(UserIndex, "You're too far away from the seller.", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'Start commerce.... */
		IniciarComercioNPC(UserIndex);
		/* '[Alejo] */
	} else if (UserList[UserIndex].flags.TargetUser > 0) {
		/* 'User commerce... */
		/* 'Can he commerce?? */
		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
			WriteConsoleMsg(UserIndex, "You can't sell items.", FontTypeNames_FONTTYPE_WARNING);
			return;
		}

		/* 'Is the other one dead?? */
		if (UserList[UserList[UserIndex].flags.TargetUser].flags.Muerto == 1) {
			WriteConsoleMsg(UserIndex, "You can't trade with the dead!!",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'Is it me?? */
		if (UserList[UserIndex].flags.TargetUser == UserIndex) {
			WriteConsoleMsg(UserIndex, "You can't trade with yourself!!", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'Check distance */
		if (Distancia(UserList[UserList[UserIndex].flags.TargetUser].Pos, UserList[UserIndex].Pos) > 3) {
			WriteConsoleMsg(UserIndex, "You're too far away from the user.", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'Is he already trading?? is it with me or someone else?? */
		if (UserList[UserList[UserIndex].flags.TargetUser].flags.Comerciando == true
				&& UserList[UserList[UserIndex].flags.TargetUser].ComUsu.DestUsu != UserIndex) {
			WriteConsoleMsg(UserIndex, "You can't trade with that user right now.",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'Initialize some variables... */
		UserList[UserIndex].ComUsu.DestUsu = UserList[UserIndex].flags.TargetUser;
		UserList[UserIndex].ComUsu.DestNick = UserList[UserList[UserIndex].flags.TargetUser].Name;
		for (i = (1); i <= (MAX_OFFER_SLOTS); i++) {
			UserList[UserIndex].ComUsu.cant[i] = 0;
			UserList[UserIndex].ComUsu.Objeto[i] = 0;
		}
		UserList[UserIndex].ComUsu.GoldAmount = 0;

		UserList[UserIndex].ComUsu.Acepto = false;
		UserList[UserIndex].ComUsu.Confirmo = false;

		/* 'Rutina para comerciar con otro usuario */
		IniciarComercioConUsuario(UserIndex, UserList[UserIndex].flags.TargetUser);
	} else {
		WriteConsoleMsg(UserIndex, "First, left-click the character.",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "BankStart" message. */
/* ' */


void HoAClientPacketHandler::handleBankStart(BankStart* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Dead people can't commerce */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (UserList[UserIndex].flags.Comerciando) {
		WriteConsoleMsg(UserIndex, "You're already trading.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC > 0) {
		if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 3) {
			WriteConsoleMsg(UserIndex, "You're too far away from the seller.", FontTypeNames_FONTTYPE_INFO);
			return;
		}

		/* 'If it's the banker.... */
		if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype == eNPCType_Banquero) {
			IniciarDeposito(UserIndex);
		}
	} else {
		WriteConsoleMsg(UserIndex, "First, left-click the PC.",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "Enlist" message. */
/* ' */


void HoAClientPacketHandler::handleEnlist(Enlist* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Noble
			|| UserList[UserIndex].flags.Muerto != 0) {
		return;
	}

	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 4) {
		WriteConsoleMsg(UserIndex, "You must come closer.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].flags.Faccion == 0) {
		EnlistarArmadaReal(UserIndex);
	} else {
		EnlistarCaos(UserIndex);
	}
}

/* '' */
/* ' Handles the "Information" message. */
/* ' */


void HoAClientPacketHandler::handleInformation(Information* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int Matados;
	int NextRecom;
	int Diferencia;




	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Noble
			|| UserList[UserIndex].flags.Muerto != 0) {
		return;
	}

	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 4) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	NextRecom = UserList[UserIndex].Faccion.NextRecompensa;

	if (Npclist[UserList[UserIndex].flags.TargetNPC].flags.Faccion == 0) {
		if (UserList[UserIndex].Faccion.ArmadaReal == 0) {
			WriteChatOverHead(UserIndex, "You're not part of the Royal Army!!",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
			return;
		}

		Matados = UserList[UserIndex].Faccion.CriminalesMatados;
		Diferencia = NextRecom - Matados;

		if (Diferencia > 0) {
			WriteChatOverHead(UserIndex,
					"Fighting criminals is your duty, kill " + vb6::CStr(Diferencia)
							+ " more criminals and I shall reward you.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
		} else {
			WriteChatOverHead(UserIndex,
					"Fighting criminals is your duty, and you've already killed enough to receive a reward.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
		}
	} else {
		if (UserList[UserIndex].Faccion.FuerzasCaos == 0) {
			WriteChatOverHead(UserIndex, "You're not part of the Dark Legion!!",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
			return;
		}

		Matados = UserList[UserIndex].Faccion.CiudadanosMatados;
		Diferencia = NextRecom - Matados;

		if (Diferencia > 0) {
			WriteChatOverHead(UserIndex,
					"Sowing chaos and despair is your duty, kill " + vb6::CStr(Diferencia)
							+ " more citizens and I shall reward you.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
		} else {
			WriteChatOverHead(UserIndex,
					"Sowing chaos and despair is your duty, and I think a reward is due.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
		}
	}
}

/* '' */
/* ' Handles the "Reward" message. */
/* ' */


void HoAClientPacketHandler::handleReward(Reward* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Noble
			|| UserList[UserIndex].flags.Muerto != 0) {
		return;
	}

	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 4) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].flags.Faccion == 0) {
		if (UserList[UserIndex].Faccion.ArmadaReal == 0) {
			WriteChatOverHead(UserIndex, "You're not part of the Royal Army!!",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
			return;
		}
		RecompensaArmadaReal(UserIndex);
	} else {
		if (UserList[UserIndex].Faccion.FuerzasCaos == 0) {
			WriteChatOverHead(UserIndex, "You're not part of the Dark Legion!!",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
			return;
		}
		RecompensaCaos(UserIndex);
	}
}

/* '' */
/* ' Handles the "RequestMOTD" message. */
/* ' */


void HoAClientPacketHandler::handleRequestMOTD(RequestMOTD* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	SendMOTD(UserIndex);
}

/* '' */
/* ' Handles the "UpTime" message. */
/* ' */


void HoAClientPacketHandler::handleUpTime(UpTime* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/10/08 */
	/* '01/10/2008 - Marcos Martinez (ByVal) - Automatic restart removed from the server along with all their assignments and varibles */
	/* '*************************************************** */



	int time;
	std::string UpTimeStr;

	/* 'Get total time in seconds */
	time = getInterval((vb6::GetTickCount()), tInicioServer) / 1000;

	/* 'Get times in dd:hh:mm:ss format */
	UpTimeStr = vb6::CStr(time % 60) + " seconds.";
	time = time / 60;

	UpTimeStr = vb6::CStr(time % 60) + " minutes, " + UpTimeStr;
	time = time / 60;

	UpTimeStr = vb6::CStr(time % 24) + " hours, " + UpTimeStr;
	time = time / 24;

	if (time == 1) {
		UpTimeStr = vb6::CStr(time) + " day, " + UpTimeStr;
	} else {
		UpTimeStr = vb6::CStr(time) + " days, " + UpTimeStr;
	}

	WriteConsoleMsg(UserIndex, "Server Online: " + UpTimeStr, FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "PartyLeave" message. */
/* ' */


void HoAClientPacketHandler::handlePartyLeave(PartyLeave* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	SalirDeParty(UserIndex);
}

/* '' */
/* ' Handles the "PartyCreate" message. */
/* ' */


void HoAClientPacketHandler::handlePartyCreate(PartyCreate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	if (!PuedeCrearParty(UserIndex)) {
		return;
	}

	CrearParty(UserIndex);
}

/* '' */
/* ' Handles the "PartyJoin" message. */
/* ' */


void HoAClientPacketHandler::handlePartyJoin(PartyJoin* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	SolicitarIngresoAParty(UserIndex);
}

/* '' */
/* ' Handles the "ShareNpc" message. */
/* ' */


void HoAClientPacketHandler::handleShareNpc(ShareNpc* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 15/04/2010 */
	/* 'Shares owned npcs with other user */
	/* '*************************************************** */

	int TargetUserIndex;
	int SharingUserIndex;




	/* ' Didn't target any user */
	TargetUserIndex = UserList[UserIndex].flags.TargetUser;
	if (TargetUserIndex == 0) {
		return;
	}

	/* ' Can't share with admins */
	if (EsGm(TargetUserIndex)) {
		WriteConsoleMsg(UserIndex, "You can't share NPCs with admins!!",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* ' Pk or Caos? */
	if (criminal(UserIndex)) {
		/* ' Caos can only share with other caos */
		if (esCaos(UserIndex)) {
			if (!esCaos(TargetUserIndex)) {
				WriteConsoleMsg(UserIndex, "You can only share NPCs with members of your own faction!!",
						FontTypeNames_FONTTYPE_INFO);
				return;
			}

			/* ' Pks don't need to share with anyone */
		} else {
			return;
		}

		/* ' Ciuda or Army? */
	} else {
		/* ' Can't share */
		if (criminal(TargetUserIndex)) {
			WriteConsoleMsg(UserIndex, "You can't share NPCs with criminals!!",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}
	}

	/* ' Already sharing with target */
	SharingUserIndex = UserList[UserIndex].flags.ShareNpcWith;
	if (SharingUserIndex == TargetUserIndex) {
		return;
	}

	/* ' Aviso al usuario anterior que dejo de compartir */
	if (SharingUserIndex != 0) {
		WriteConsoleMsg(SharingUserIndex,
				UserList[UserIndex].Name + " has stopped sharing NPCs with you.",
				FontTypeNames_FONTTYPE_INFO);
		WriteConsoleMsg(UserIndex,
				"You've stopped sharing your NPCs with " + UserList[SharingUserIndex].Name + ".",
				FontTypeNames_FONTTYPE_INFO);
	}

	UserList[UserIndex].flags.ShareNpcWith = TargetUserIndex;

	WriteConsoleMsg(TargetUserIndex, UserList[UserIndex].Name + " now shares NPCs with you.",
			FontTypeNames_FONTTYPE_INFO);
	WriteConsoleMsg(UserIndex, "You are now sharing your NPCs with " + UserList[TargetUserIndex].Name + ".",
			FontTypeNames_FONTTYPE_INFO);

}

/* '' */
/* ' Handles the "StopSharingNpc" message. */
/* ' */


void HoAClientPacketHandler::handleStopSharingNpc(StopSharingNpc* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 15/04/2010 */
	/* 'Stop Sharing owned npcs with other user */
	/* '*************************************************** */

	int SharingUserIndex;




	SharingUserIndex = UserList[UserIndex].flags.ShareNpcWith;

	if (SharingUserIndex != 0) {

		/* ' Aviso al que compartia y al que le compartia. */
		WriteConsoleMsg(SharingUserIndex,
				UserList[UserIndex].Name + " has stopped sharing their NPCs with you.",
				FontTypeNames_FONTTYPE_INFO);
		WriteConsoleMsg(SharingUserIndex,
				"You've stopped sahring your NPCs with " + UserList[SharingUserIndex].Name + ".",
				FontTypeNames_FONTTYPE_INFO);

		UserList[UserIndex].flags.ShareNpcWith = 0;
	}

}

/* '' */
/* ' Handles the "Inquiry" message. */
/* ' */


void HoAClientPacketHandler::handleInquiry(Inquiry* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	ConsultaPopular->SendInfoEncuesta(UserIndex);
}

/* '' */
/* ' Handles the "GuildMessage" message. */
/* ' */


void HoAClientPacketHandler::handleGuildMessage(GuildMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 15/07/2009 */
	/* '02/03/2009: ZaMa - Arreglado un indice mal pasado a la funcion de cartel de clanes overhead. */
	/* '15/07/2009: ZaMa - Now invisible admins only speak by console */
	/* '*************************************************** */

	std::string Chat;

	Chat = p->Chat;

	if (vb6::LenB(Chat) != 0) {
		/* 'Analize chat... */
		ParseChat(Chat);

		if (UserList[UserIndex].GuildIndex > 0) {
			SendData(SendTarget_ToDiosesYclan, UserList[UserIndex].GuildIndex,
					hoa::protocol::server::BuildGuildChat(UserList[UserIndex].Name + "> " + Chat));

			if (!(UserList[UserIndex].flags.AdminInvisible == 1)) {
				SendData(SendTarget_ToClanArea, UserIndex,
						BuildChatOverHead("< " + Chat + " >", UserList[UserIndex].Char.CharIndex,
								vbYellow));
			}
		}
	}




}

/* '' */
/* ' Handles the "PartyMessage" message. */
/* ' */


void HoAClientPacketHandler::handlePartyMessage(PartyMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string Chat;

	Chat = p->Chat;

	if (vb6::LenB(Chat) != 0) {
		/* 'Analize chat... */
		ParseChat(Chat);

		BroadCastParty(UserIndex, Chat);
		/* 'TODO : Con la 0.12.1 se debe definir si esto vuelve o se borra (/CMSG overhead) */
		/* 'Call SendData(SendTarget.ToPartyArea, UserIndex, UserList(UserIndex).Pos.map, "||" & vbYellow & "°< " & mid$(rData, 7) & " >°" & CStr(UserList(UserIndex).Char.CharIndex)) */
	}




}

/* '' */
/* ' Handles the "CentinelReport" message. */
/* ' */


void HoAClientPacketHandler::handleCentinelReport(CentinelReport* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	CentinelaCheckClave(UserIndex, p->Code);
}

/* '' */
/* ' Handles the "GuildOnline" message. */
/* ' */


void HoAClientPacketHandler::handleGuildOnline(GuildOnline* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string onlineList;

	onlineList = m_ListaDeMiembrosOnline(UserIndex, UserList[UserIndex].GuildIndex);

	if (UserList[UserIndex].GuildIndex != 0) {
		WriteConsoleMsg(UserIndex, "Connected fellow clan members: " + onlineList,
				FontTypeNames_FONTTYPE_GUILDMSG);
	} else {
		WriteConsoleMsg(UserIndex, "You're not part of a clan.", FontTypeNames_FONTTYPE_GUILDMSG);
	}
}

/* '' */
/* ' Handles the "PartyOnline" message. */
/* ' */


void HoAClientPacketHandler::handlePartyOnline(PartyOnline* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	OnlineParty(UserIndex);
}

/* '' */
/* ' Handles the "CouncilMessage" message. */
/* ' */


void HoAClientPacketHandler::handleCouncilMessage(CouncilMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string Chat;

	Chat = p->Chat;

	if (vb6::LenB(Chat) != 0) {
		/* 'Analize chat... */
		ParseChat(Chat);

		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoyalCouncil)) {
			SendData(SendTarget_ToConsejo, UserIndex,
					hoa::protocol::server::BuildConsoleMsg("(Councilman) " + UserList[UserIndex].Name + "> " + Chat,
							FontTypeNames_FONTTYPE_CONSEJO));
		} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_ChaosCouncil)) {
			SendData(SendTarget_ToConsejoCaos, UserIndex,
					hoa::protocol::server::BuildConsoleMsg("(Councilman) " + UserList[UserIndex].Name + "> " + Chat,
							FontTypeNames_FONTTYPE_CONSEJOCAOS));
		}
	}



}

/* '' */
/* ' Handles the "RoleMasterRequest" message. */
/* ' */


void HoAClientPacketHandler::handleRoleMasterRequest(RoleMasterRequest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string request;

	request = p->Request;

	if (vb6::LenB(request) != 0) {
		WriteConsoleMsg(UserIndex, "Your request has been sent.", FontTypeNames_FONTTYPE_INFO);
		SendData(SendTarget_ToRMsAndHigherAdmins, 0,
				hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + " ROLE PLAYING QUESTION: " + request,
						FontTypeNames_FONTTYPE_GUILDMSG));
	}
}

/* '' */
/* ' Handles the "GMRequest" message. */
/* ' */


void HoAClientPacketHandler::handleGMRequest(GMRequest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	auto it = std::find(Ayuda.begin(), Ayuda.end(), UserList[UserIndex].Name);
	if (it == Ayuda.end()) {
		WriteConsoleMsg(UserIndex,
				"The message has been delivered, you now need to wait until a GM is free to take your request.",
				FontTypeNames_FONTTYPE_INFO);
		Ayuda.push_back(UserList[UserIndex].Name);
	} else {
		Ayuda.erase(it);
		Ayuda.push_back(UserList[UserIndex].Name);
		WriteConsoleMsg(UserIndex,
				"You've already sent a message, your message has been moved to the end of the queue.",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "BugReport" message. */
/* ' */


void HoAClientPacketHandler::handleBugReport(BugReport* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string bugReport;

	bugReport = p->Report;

	LogBugReport(UserIndex, bugReport);

}

/* '' */
/* ' Handles the "ChangeDescription" message. */
/* ' */


void HoAClientPacketHandler::handleChangeDescription(ChangeDescription* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string description;

	description = p->Description;

	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You can't change your description while you are dead.",
				FontTypeNames_FONTTYPE_INFO);
	} else {
		if (!AsciiValidos(description)) {
			WriteConsoleMsg(UserIndex, "Your description contains invalid characters.",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			UserList[UserIndex].desc = vb6::Trim(description);
			WriteConsoleMsg(UserIndex, "Your decsription has changed.", FontTypeNames_FONTTYPE_INFO);
		}
	}

}

/* '' */
/* ' Handles the "GuildVote" message. */
/* ' */


void HoAClientPacketHandler::handleGuildVote(GuildVote* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string vote;
	std::string errorStr;

	vote = p->Vote;

	if (!v_UsuarioVota(UserIndex, vote, errorStr)) {
		WriteConsoleMsg(UserIndex, "Vote NOT counted: " + errorStr, FontTypeNames_FONTTYPE_GUILD);
	} else {
		WriteConsoleMsg(UserIndex, "Vote counted.", FontTypeNames_FONTTYPE_GUILD);
	}




}

/* '' */
/* ' Handles the "ShowGuildNews" message. */
/* ' */


void HoAClientPacketHandler::handleShowGuildNews(ShowGuildNews* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMA */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */




	SendGuildNews(UserIndex);
}

/* '' */
/* ' Handles the "Punishments" message. */
/* ' */


void HoAClientPacketHandler::handlePunishments(Punishments* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 25/08/2009 */
	/* '25/08/2009: ZaMa - Now only admins can see other admins' punishment list */
	/* '*************************************************** */

	std::string Name;
	int Count;

	Name = p->Name;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	if (vb6::LenB(Name) != 0) {
		if ((vb6::InStrB(Name, "/") != 0)) {
			Name = vb6::Replace(Name, "/", "");
		}
		if ((vb6::InStrB(Name, "/") != 0)) {
			Name = vb6::Replace(Name, "/", "");
		}
		if ((vb6::InStrB(Name, ":") != 0)) {
			Name = vb6::Replace(Name, ":", "");
		}
		if ((vb6::InStrB(Name, "|") != 0)) {
			Name = vb6::Replace(Name, "|", "");
		}

		if ((EsAdmin(Name) || EsDios(Name) || EsSemiDios(Name) || EsConsejero(Name) || EsRolesMaster(Name))
				&& (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User))) {
			WriteConsoleMsg(UserIndex, "You can't see admin punishment records.",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			if (FileExist(GetCharPath(Name), 0)) {
				Count = vb6::val(GetVar(GetCharPath(Name), "PENAS", "Cant"));
				if (Count <= 0) {
					WriteConsoleMsg(UserIndex, "No record..", FontTypeNames_FONTTYPE_INFO);
				} else {
					Count = vb6::Constrain(Count, 0, MAX_PENAS);
					while (Count > 0) {
						WriteConsoleMsg(UserIndex,
								vb6::CStr(Count) + " - " + GetVar(GetCharPath(Name), "PENAS", "P" + vb6::CStr(Count)),
								FontTypeNames_FONTTYPE_INFO);
						Count = Count - 1;
					}
				}
			} else {
				WriteConsoleMsg(UserIndex, "Unknown user \"" + Name + "\".",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}
}

/* '' */
/* ' Handles the "Gamble" message. */
/* ' */


void HoAClientPacketHandler::handleGamble(Gamble* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* '10/07/2010: ZaMa - Now normal npcs don't answer if asked to gamble. */
	/* '*************************************************** */

	int Amount;

	Amount = p->Amount;

	/* ' Dead? */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);

		/* 'Validate target NPC */
	} else if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);

		/* ' Validate Distance */
	} else if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);

		/* ' Validate NpcType */
	} else if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Timbero) {

		eNPCType TargetNpcType;
		TargetNpcType = Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype;

		/* ' Normal npcs don't speak */
		if (TargetNpcType != eNPCType_Comun && TargetNpcType != eNPCType_DRAGON
				&& TargetNpcType != eNPCType_Pretoriano) {
			WriteChatOverHead(UserIndex, "I'm not interested in gambling.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
		}

		/* ' Validate amount */
	} else if (Amount < 1) {
		WriteChatOverHead(UserIndex, "The minimum bet is 1gp.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

		/* ' Validate amount */
	} else if (Amount > 5000) {
		WriteChatOverHead(UserIndex, "The maximum bet is 5000 gp.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

		/* ' Validate user gold */
	} else if (UserList[UserIndex].Stats.GLD < Amount) {
		WriteChatOverHead(UserIndex, "You don't have that amount.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

	} else {
		if (RandomNumber(1, 100) <= 47) {
			UserList[UserIndex].Stats.GLD = UserList[UserIndex].Stats.GLD + Amount;
			WriteChatOverHead(UserIndex, "Congratulations! You've won " + vb6::CStr(Amount) + " gp.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

			Apuestas.Perdidas = Apuestas.Perdidas + Amount;
			WriteVar(GetDatPath(DATPATH::apuestas), "Main", "Losses", vb6::CStr(Apuestas.Perdidas));
		} else {
			UserList[UserIndex].Stats.GLD = UserList[UserIndex].Stats.GLD - Amount;
			WriteChatOverHead(UserIndex, "I'm sorry, you've lost " + vb6::CStr(Amount) + " gp.",
					Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

			Apuestas.Ganancias = Apuestas.Ganancias + Amount;
			WriteVar(GetDatPath(DATPATH::apuestas), "Main", "Gains", vb6::CStr(Apuestas.Ganancias));
		}

		Apuestas.Jugadas = Apuestas.Jugadas + 1;

		WriteVar(GetDatPath(DATPATH::apuestas), "Main", "Bets", vb6::CStr(Apuestas.Jugadas));

		WriteUpdateGold(UserIndex);
	}
}

/* '' */
/* ' Handles the "InquiryVote" message. */
/* ' */


void HoAClientPacketHandler::handleInquiryVote(InquiryVote* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int opt;

	opt = p->Opt;

	WriteConsoleMsg(UserIndex, ConsultaPopular->doVotar(UserIndex, opt), FontTypeNames_FONTTYPE_GUILD);
}

/* '' */
/* ' Handles the "BankExtractGold" message. */
/* ' */


void HoAClientPacketHandler::handleBankExtractGold(BankExtractGold* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Amount;

	Amount = p->Amount;

	/* 'Dead people can't leave a faction.. they can't talk... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Banquero) {
		return;
	}

	if (Distancia(UserList[UserIndex].Pos, Npclist[UserList[UserIndex].flags.TargetNPC].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Amount > 0 && Amount <= UserList[UserIndex].Stats.Banco) {
		UserList[UserIndex].Stats.Banco = UserList[UserIndex].Stats.Banco - Amount;
		UserList[UserIndex].Stats.GLD = UserList[UserIndex].Stats.GLD + Amount;
		WriteChatOverHead(UserIndex,
				"You have " + vb6::CStr(UserList[UserIndex].Stats.Banco) + " gp in your account.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
	} else {
		WriteChatOverHead(UserIndex, "You don't have that amount.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
	}

	WriteUpdateGold(UserIndex);
	WriteUpdateBankGold(UserIndex);
}

/* '' */
/* ' Handles the "LeaveFaction" message. */
/* ' */


void HoAClientPacketHandler::handleLeaveFaction(LeaveFaction* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 09/28/2010 */
	/* ' 09/28/2010 C4b3z0n - Ahora la respuesta de los NPCs sino perteneces a ninguna facción solo la hacen el Rey o el Demonio */
	/* ' 05/17/06 - Maraxus */
	/* '*************************************************** */

	bool TalkToKing = false;
	bool TalkToDemon = false;
	int NpcIndex = 0;




	/* 'Dead people can't leave a faction.. they can't talk... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* ' Chequea si habla con el rey o el demonio. Puede salir sin hacerlo, pero si lo hace le reponden los npcs */
	NpcIndex = UserList[UserIndex].flags.TargetNPC;
	if (NpcIndex != 0) {
		/* ' Es rey o domonio? */
		if (Npclist[NpcIndex].NPCtype == eNPCType_Noble) {
			/* 'Rey? */
			if (Npclist[NpcIndex].flags.Faccion == 0) {
				TalkToKing = true;
				/* ' Demonio */
			} else {
				TalkToDemon = true;
			}
		}
	}

	/* 'Quit the Royal Army? */
	if (UserList[UserIndex].Faccion.ArmadaReal == 1) {
		/* ' Si le pidio al demonio salir de la armada, este le responde. */
		if (TalkToDemon) {
			if (NpcIndex > 0) {
				WriteChatOverHead(UserIndex, "Leave you fool!!!", Npclist[NpcIndex].Char.CharIndex,
					0x00ffffff);
			}

		} else {
			/* ' Si le pidio al rey salir de la armada, le responde. */
			if (TalkToKing) {
				if (NpcIndex > 0) {
					WriteChatOverHead(UserIndex, "You'll be welcome to the Royal Army if you wish to return.",
						Npclist[NpcIndex].Char.CharIndex, 0x00ffffff);
				}
			}

			ExpulsarFaccionReal(UserIndex, false);

		}

		/* 'Quit the Chaos Legion? */
	} else if (UserList[UserIndex].Faccion.FuerzasCaos == 1) {
		/* ' Si le pidio al rey salir del caos, le responde. */
		if (TalkToKing) {
			if (NpcIndex > 0) {
				WriteChatOverHead(UserIndex, "Leave you damn criminal!!!",
						Npclist[NpcIndex].Char.CharIndex, 0x00ffffff);
			}
		} else {
			/* ' Si le pidio al demonio salir del caos, este le responde. */
			if (TalkToDemon) {
				if (NpcIndex > 0) {
					WriteChatOverHead(UserIndex, "You'll crawl back.", Npclist[NpcIndex].Char.CharIndex,
							0x00ffffff);
				}
			}

			ExpulsarFaccionCaos(UserIndex, false);
		}
		/* ' No es faccionario */
	} else {

		/* ' Si le hablaba al rey o demonio, le repsonden ellos */
		/* 'Corregido, solo si son en efecto el rey o el demonio, no cualquier NPC (C4b3z0n) */
		/* 'Si se pueden unir a la facción (status), son invitados */
		if ((TalkToDemon && criminal(UserIndex)) || (TalkToKing && !criminal(UserIndex))) {
			if (NpcIndex > 0) {
				WriteChatOverHead(UserIndex, "You're not part of our faction. If you wish to join, say /ENLIST",
						Npclist[NpcIndex].Char.CharIndex, 0x00ffffff);
			}
		} else if ((TalkToDemon && !criminal(UserIndex))) {
			if (NpcIndex > 0) {
				WriteChatOverHead(UserIndex, "Leave you fool!!!", Npclist[NpcIndex].Char.CharIndex,
						0x00ffffff);
			}
		} else if ((TalkToKing && criminal(UserIndex))) {
			if (NpcIndex > 0) {
				WriteChatOverHead(UserIndex, "Leave you damn criminal!!!",
						Npclist[NpcIndex].Char.CharIndex, 0x00ffffff);
			}
		} else {
			WriteConsoleMsg(UserIndex, "You're not part of any faction!", FontTypeNames_FONTTYPE_FIGHT);
		}

	}

}

/* '' */
/* ' Handles the "BankDepositGold" message. */
/* ' */


void HoAClientPacketHandler::handleBankDepositGold(BankDepositGold* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	int Amount;

	Amount = p->Amount;

	/* 'Dead people can't leave a faction.. they can't talk... */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	/* 'Validate target NPC */
	if (UserList[UserIndex].flags.TargetNPC == 0) {
		WriteConsoleMsg(UserIndex,
				"First left-click a PC",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Distancia(Npclist[UserList[UserIndex].flags.TargetNPC].Pos, UserList[UserIndex].Pos) > 10) {
		WriteConsoleMsg(UserIndex, "You're too far away.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (Npclist[UserList[UserIndex].flags.TargetNPC].NPCtype != eNPCType_Banquero) {
		return;
	}

	if (Amount > 0 && Amount <= UserList[UserIndex].Stats.GLD) {
		UserList[UserIndex].Stats.Banco = UserList[UserIndex].Stats.Banco + Amount;
		UserList[UserIndex].Stats.GLD = UserList[UserIndex].Stats.GLD - Amount;
		WriteChatOverHead(UserIndex,
				"You have " + vb6::CStr(UserList[UserIndex].Stats.Banco) + " gp in your account.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);

		WriteUpdateGold(UserIndex);
		WriteUpdateBankGold(UserIndex);
	} else {
		WriteChatOverHead(UserIndex, "You don't have that amount.",
				Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff);
	}
}

/* '' */
/* ' Handles the "Denounce" message. */
/* ' */


void HoAClientPacketHandler::handleDenounce(Denounce* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 14/11/2010 */
	/* '14/11/2010: ZaMa - Now denounces can be desactivated. */
	/* '*************************************************** */

	std::string Text;
	std::string msg;

	Text = p->Text;

	if (UserList[UserIndex].flags.Silenciado == 0) {
		/* 'Analize chat... */
		ParseChat(Text);

		msg = vb6::LCase(UserList[UserIndex].Name) + " COMPLAINT: " + Text;

		SendData(SendTarget_ToAdmins, 0, hoa::protocol::server::BuildConsoleMsg(msg, FontTypeNames_FONTTYPE_GUILDMSG),
				true);

		Denuncias.push_back(msg);
		LogDesarrollo("Complaint from " + UserList[UserIndex].Name + ": " + msg);

		WriteConsoleMsg(UserIndex, "Complaint sent, please wait..", FontTypeNames_FONTTYPE_INFO);
	}




}

/* '' */
/* ' Handles the "GuildFundate" message. */
/* ' */


void HoAClientPacketHandler::handleGuildFundate(GuildFundate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/12/2009 */
	/* ' */
	/* '*************************************************** */

	if (HasFound(UserList[UserIndex].Name)) {
		WriteConsoleMsg(UserIndex, "A clan you've already founded, impossible to found a second!",
				FontTypeNames_FONTTYPE_INFOBOLD);
		return;
	}
	
	WriteShowGuildAlign(UserIndex);
}

/* '' */
/* ' Handles the "GuildFundation" message. */
/* ' */


void HoAClientPacketHandler::handleGuildFundation(GuildFundation* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/12/2009 */
	/* ' */
	/* '*************************************************** */

	eClanType clanType;
	std::string ERROR;

	clanType = static_cast<eClanType>(p->ClanType);

	if (HasFound(UserList[UserIndex].Name)) {
		WriteConsoleMsg(UserIndex, "A clan you've already founded, impossible to found a second!",
				FontTypeNames_FONTTYPE_INFOBOLD);
		LogCheating(
				"User " + UserList[UserIndex].Name
						+ " has tried to found a clan having already founded one from IP "
						+ UserList[UserIndex].ip);
		return;
	}

	switch (clanType) {
	case eClanType_ct_RoyalArmy:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_ARMADA;
		break;

	case eClanType_ct_Evil:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_LEGION;
		break;

	case eClanType_ct_Neutral:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_NEUTRO;
		break;

	case eClanType_ct_GM:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_MASTER;
		break;

	case eClanType_ct_Legal:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_CIUDA;
		break;

	case eClanType_ct_Criminal:
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_ALINEACION_CRIMINAL;
		break;

	default:
		WriteConsoleMsg(UserIndex, "Invalid Alignment.", FontTypeNames_FONTTYPE_GUILD);
		return;
		break;
	}

	if (PuedeFundarUnClan(UserIndex, UserList[UserIndex].FundandoGuildAlineacion, ERROR)) {
		WriteShowGuildFundationForm(UserIndex);
	} else {
		UserList[UserIndex].FundandoGuildAlineacion = ALINEACION_GUILD_Null;
		WriteConsoleMsg(UserIndex, ERROR, FontTypeNames_FONTTYPE_GUILD);
	}
}

/* '' */
/* ' Handles the "PartyKick" message. */
/* ' */


void HoAClientPacketHandler::handlePartyKick(PartyKick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/05/09 */
	/* 'Last Modification by: Marco Vanotti (Marco) */
	/* '- 05/05/09: Now it uses "UserPuedeEjecutarComandos" to check if the user can use party commands */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (UserPuedeEjecutarComandos(UserIndex)) {
		tUser = NameIndex(UserName);

		if (tUser > 0) {
			ExpulsarDeParty(UserIndex, tUser);
		} else {
			if (vb6::InStr(UserName, "+")) {
				UserName = vb6::Replace(UserName, "+", " ");
			}

			WriteConsoleMsg(UserIndex, vb6::LCase(UserName) + " does not belong to your party.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}




}

/* '' */
/* ' Handles the "PartySetLeader" message. */
/* ' */


void HoAClientPacketHandler::handlePartySetLeader(PartySetLeader* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/05/09 */
	/* 'Last Modification by: Marco Vanotti (MarKoxX) */
	/* '- 05/05/09: Now it uses "UserPuedeEjecutarComandos" to check if the user can use party commands */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	//int rank;
	//rank = PlayerType_Admin || PlayerType_Dios || PlayerType_SemiDios || PlayerType_Consejero;

	UserName = p->UserName;

	if (UserPuedeEjecutarComandos(UserIndex)) {
		tUser = NameIndex(UserName);
		if (tUser > 0) {
			/* 'Don't allow users to spoof online GMs */
			if (!UserTieneMasPrivilegiosQue(UserName, UserIndex)) {
				TransformarEnLider(UserIndex, tUser);
			} else {
				WriteConsoleMsg(UserIndex, vb6::LCase(UserList[tUser].Name) + " does not belong to your party",
						FontTypeNames_FONTTYPE_INFO);
			}

		} else {
			if (vb6::InStr(UserName, "+")) {
				UserName = vb6::Replace(UserName, "+", " ");
			}
			WriteConsoleMsg(UserIndex, vb6::LCase(UserName) + " does not belong to your party",
					FontTypeNames_FONTTYPE_INFO);
		}
	}




}

/* '' */
/* ' Handles the "PartyAcceptMember" message. */
/* ' */


void HoAClientPacketHandler::handlePartyAcceptMember(PartyAcceptMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/05/09 */
	/* 'Last Modification by: Marco Vanotti (Marco) */
	/* '- 05/05/09: Now it uses "UserPuedeEjecutarComandos" to check if the user can use party commands */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	bool bUserVivo = false;

	UserName = p->UserName;

	if (UserList[UserIndex].flags.Muerto) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_PARTY);
	} else {
		bUserVivo = true;
	}

	if (UserPuedeEjecutarComandos(UserIndex) && bUserVivo) {
		tUser = NameIndex(UserName);
		if (tUser > 0) {
			/* 'Validate administrative ranks - don't allow users to spoof online GMs */
			if (!UserTieneMasPrivilegiosQue(tUser, UserIndex)) {
				AprobarIngresoAParty(UserIndex, tUser);
			} else {
				WriteConsoleMsg(UserIndex, "You can't add PCs with a greater hierarchy to your party.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (vb6::InStr(UserName, "+")) {
				UserName = vb6::Replace(UserName, "+", " ");
			}

			/* 'Don't allow users to spoof online GMs */
			if (!UserTieneMasPrivilegiosQue(UserName, UserIndex)) {
				WriteConsoleMsg(UserIndex, vb6::LCase(UserName) + " has not requested to enter your party.",
						FontTypeNames_FONTTYPE_PARTY);
			} else {
				WriteConsoleMsg(UserIndex, "You can't add PCs with a greater hierarchy to your party.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "GuildMemberList" message. */
/* ' */


void HoAClientPacketHandler::handleGuildMemberList(GuildMemberList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string guild;
	int memberCount;
	int i;
	std::string UserName;

	guild = p->GuildName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if ((vb6::InStrB(guild, "/") != 0)) {
			guild = vb6::Replace(guild, "/", "");
		}
		if ((vb6::InStrB(guild, "/") != 0)) {
			guild = vb6::Replace(guild, "/", "");
		}

		if (!FileExist(GetGuildsPath(guild, EGUILDPATH::Members))) {
			WriteConsoleMsg(UserIndex, "Clan does not exist: " + guild, FontTypeNames_FONTTYPE_INFO);
		} else {
			memberCount = vb6::val(
					GetVar(GetGuildsPath(guild, EGUILDPATH::Members), "INIT",
							"NroMembers"));
			memberCount = vb6::Constrain(memberCount, 0, MAXCLANMEMBERS);

			for (i = (1); i <= (memberCount); i++) {
				UserName = GetVar(GetGuildsPath(guild, EGUILDPATH::Members), "Members",
						"Member" + vb6::CStr(i));

				WriteConsoleMsg(UserIndex, UserName + "<" + guild + ">", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "GMMessage" message. */
/* ' */


void HoAClientPacketHandler::handleGMMessage(GMMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/08/07 */
	/* 'Last Modification by: (liquid) */
	/* '*************************************************** */

	std::string message;

	message = p->Chat;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		LogGM(UserList[UserIndex].Name, "Message to GMs:" + message);

		if (vb6::LenB(message) != 0) {
			/* 'Analize chat... */
			ParseChat(message);

			SendData(SendTarget_ToAdmins, 0,
					hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + "> " + message,
							FontTypeNames_FONTTYPE_GMMSG));
		}
	}




}

/* '' */
/* ' Handles the "ShowName" message. */
/* ' */


void HoAClientPacketHandler::handleShowName(ShowName* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		/* 'Show / Hide the name */
		UserList[UserIndex].showName = !UserList[UserIndex].showName;

		RefreshCharStatus(UserIndex);
	}
}

/* '' */
/* ' Handles the "OnlineRoyalArmy" message. */
/* ' */


void HoAClientPacketHandler::handleOnlineRoyalArmy(OnlineRoyalArmy* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 28/05/2010 */
	/* '28/05/2010: ZaMa - Ahora solo dioses pueden ver otros dioses online. */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	int i;
	std::string list;
	bool esgm = false;

	/* ' Solo dioses pueden ver otros dioses online */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		esgm = true;
	}

	for (i = (1); i <= (LastUser); i++) {
		if (UserIndexSocketValido(i)) {
			if (UserList[i].Faccion.ArmadaReal == 1) {
				if (esgm || UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
					list = list + UserList[i].Name + ", ";
				}
			}
		}
	}

	if (vb6::Len(list) > 0) {
		WriteConsoleMsg(UserIndex, "Royal Army members connected: " + vb6::Left(list, vb6::Len(list) - 2),
				FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "There are no members of the Royal Army connected.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "OnlineChaosLegion" message. */
/* ' */


void HoAClientPacketHandler::handleOnlineChaosLegion(OnlineChaosLegion* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 28/05/2010 */
	/* '28/05/2010: ZaMa - Ahora solo dioses pueden ver otros dioses online. */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	int i;
	std::string list;
	bool esgm = false;

	/* ' Solo dioses pueden ver otros dioses online */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		esgm = true;
	}

	for (i = (1); i <= (LastUser); i++) {
		if (UserIndexSocketValido(i)) {
			if (UserList[i].Faccion.FuerzasCaos == 1) {
				if (UserTieneAlgunPrivilegios(i, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios) || esgm) {
					list = list + UserList[i].Name + ", ";
				}
			}
		}
	}

	if (vb6::Len(list) > 0) {
		WriteConsoleMsg(UserIndex, "Dark Legion members connected: " + vb6::Left(list, vb6::Len(list) - 2),
				FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "There are no members of the Dark Legion connected.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "GoNearby" message. */
/* ' */


void HoAClientPacketHandler::handleGoNearby(GoNearby* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/10/07 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;

	UserName = p->UserName;

	int tIndex;
	int X;
	int Y;
	int i;
	bool Found = false;

	tIndex = NameIndex(UserName);

	/* 'Check the user has enough powers */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios, PlayerType_Consejero)) {
		/* 'Si es dios o Admins no podemos salvo que nosotros también lo seamos */
		if (!(EsDios(UserName) || EsAdmin(UserName))
				|| (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin))) {
			/* 'existe el usuario destino? */
			if (tIndex <= 0) {
				WriteConsoleMsg(UserIndex, "User is offline.", FontTypeNames_FONTTYPE_INFO);
			} else {
				/* 'esto for sirve ir cambiando la distancia destino */
				for (i = (2); i <= (5); i++) {
					for (X = (UserList[tIndex].Pos.X - i); X <= (UserList[tIndex].Pos.X + i); X++) {
						for (Y = (UserList[tIndex].Pos.Y - i); Y <= (UserList[tIndex].Pos.Y + i); Y++) {
							if (MapData[UserList[tIndex].Pos.Map][X][Y].UserIndex == 0) {
								if (LegalPos(UserList[tIndex].Pos.Map, X, Y, true, true)) {
									WarpUserChar(UserIndex, UserList[tIndex].Pos.Map, X, Y, true);
									LogGM(UserList[UserIndex].Name,
											"/IRCERCA " + UserName + " Map:" + vb6::CStr(UserList[tIndex].Pos.Map)
													+ " X:" + vb6::CStr(UserList[tIndex].Pos.X) + " Y:"
													+ vb6::CStr(UserList[tIndex].Pos.Y));
									Found = true;
									break;
								}
							}
						}

						/* ' Feo, pero hay que abortar 3 fors sin usar GoTo */
						if (Found) {
							break;
						}
					}

					/* ' Feo, pero hay que abortar 3 fors sin usar GoTo */
					if (Found) {
						break;
					}
				}

				/* 'No space found?? */
				if (!Found) {
					WriteConsoleMsg(UserIndex, "All spaces are occupied.",
							FontTypeNames_FONTTYPE_INFO);
				}
			}
		}
	}




}

/* '' */
/* ' Handles the "Comment" message. */
/* ' */


void HoAClientPacketHandler::handleComment(Comment* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string& comment = p->Data;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		LogGM(UserList[UserIndex].Name, "Comment: " + comment);
		WriteConsoleMsg(UserIndex, "Comment saved...", FontTypeNames_FONTTYPE_INFO);
	}




}

/* '' */
/* ' Handles the "ServerTime" message. */
/* ' */


void HoAClientPacketHandler::handleServerTime(ServerTime* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/08/07 */
	/* 'Last Modification by: (liquid) */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, "Time.");

	SendData(SendTarget_ToAll, 0,
			hoa::protocol::server::BuildConsoleMsg("Time: " + vb6::dateToString(vb6::Now()), FontTypeNames_FONTTYPE_INFO));
}

/* '' */
/* ' Handles the "Where" message. */
/* ' */


void HoAClientPacketHandler::handleWhere(Where* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 18/11/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '18/11/2010: ZaMa - Obtengo los privs del charfile antes de mostrar la posicion de un usuario offline. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	std::string miPos;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {

		tUser = NameIndex(UserName);
		if (tUser <= 0) {

			if (FileExist(GetCharPath(UserName), 0)) {

				PlayerType CharPrivs;
				CharPrivs = GetCharPrivs(UserName);

				if ((CharPrivs & (PlayerType_User | PlayerType_Consejero | PlayerType_SemiDios)) != 0
						|| (((CharPrivs & (PlayerType_Dios | PlayerType_Admin)) != 0)
								&& (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)))) {
					miPos = GetVar(GetCharPath(UserName), "INIT", "POSITION");
					WriteConsoleMsg(UserIndex,
							"Position  " + UserName + " (Offline): " + ReadField(1, miPos, 45) + ", "
									+ ReadField(2, miPos, 45) + ", " + ReadField(3, miPos, 45) + ".",
							FontTypeNames_FONTTYPE_INFO);
				}
			} else {
				if (!(EsDios(UserName) || EsAdmin(UserName))) {
					WriteConsoleMsg(UserIndex, "Unknown user.", FontTypeNames_FONTTYPE_INFO);
				} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
					WriteConsoleMsg(UserIndex, "Unknown user.", FontTypeNames_FONTTYPE_INFO);
				}
			}
		} else {
			if ((UserTieneAlgunPrivilegios(tUser, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios))
					|| (((UserTieneAlgunPrivilegios(tUser, PlayerType_Dios, PlayerType_Admin)))
							&& (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)))) {
				WriteConsoleMsg(UserIndex,
						"Position  " + UserName + ": " + vb6::CStr(UserList[tUser].Pos.Map) + ", "
								+ vb6::CStr(UserList[tUser].Pos.X) + ", " + vb6::CStr(UserList[tUser].Pos.Y) + ".",
						FontTypeNames_FONTTYPE_INFO);
			}
		}

		LogGM(UserList[UserIndex].Name, "/Donde " + UserName);
	}




}

/* '' */
/* ' Handles the "CreaturesInMap" message. */
/* ' */


void HoAClientPacketHandler::handleCreaturesInMap(CreaturesInMap* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 30/07/06 */
	/* 'Pablo (ToxicWaste): modificaciones generales para simplificar la visualización. */
	/* '*************************************************** */

	int Map;
	int i;
	int j;
	int NPCcount1 = 0;
	int NPCcount2 = 0;
	vb6::array<int> NPCcant1;
	vb6::array<int> NPCcant2;
	vb6::array<std::string> List1;
	vb6::array<std::string> List2;

	Map = p->Map;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	if (MapaValido(Map)) {
		for (i = (1); i <= (LastNPC); i++) {
			/* 'VB isn't lazzy, so we put more restrictive condition first to speed up the process */
			if (Npclist[i].Pos.Map == Map) {
				/* '¿esta vivo? */
				if (Npclist[i].flags.NPCActive && Npclist[i].Hostile == 1
						&& Npclist[i].Stats.Alineacion == 2) {
					if (NPCcount1 == 0) {
						List1.redim(0);
						List1.redim(0);
						NPCcant1.redim(0);
						NPCcant1.redim(0);
						NPCcount1 = 1;
						List1[0] = Npclist[i].Name + ": (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y) + ")";
						NPCcant1[0] = 1;
					} else {
						for (j = (0); j <= (NPCcount1 - 1); j++) {
							if (vb6::Left(List1[j], vb6::Len(Npclist[i].Name)) == Npclist[i].Name) {
								List1[j] = List1[j] + ", (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y) + ")";
								NPCcant1[j] = NPCcant1[j] + 1;
								break;
							}
						}
						if (j == NPCcount1) {
							List1.redim(NPCcount1);
							NPCcant1.redim(NPCcount1);
							NPCcount1 = NPCcount1 + 1;
							List1[j] = Npclist[i].Name + ": (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y)
									+ ")";
							NPCcant1[j] = 1;
						}
					}
				} else {
					if (NPCcount2 == 0) {
						List2.redim(0);
						List2.redim(0);
						NPCcant2.redim(0);
						NPCcant2.redim(0);
						NPCcount2 = 1;
						List2[0] = Npclist[i].Name + ": (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y) + ")";
						NPCcant2[0] = 1;
					} else {
						for (j = (0); j <= (NPCcount2 - 1); j++) {
							if (vb6::Left(List2[j], vb6::Len(Npclist[i].Name)) == Npclist[i].Name) {
								List2[j] = List2[j] + ", (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y) + ")";
								NPCcant2[j] = NPCcant2[j] + 1;
								break;
							}
						}
						if (j == NPCcount2) {
							List2.redim(NPCcount2);
							NPCcant2.redim(NPCcount2);
							NPCcount2 = NPCcount2 + 1;
							List2[j] = Npclist[i].Name + ": (" + vb6::CStr(Npclist[i].Pos.X) + "," + vb6::CStr(Npclist[i].Pos.Y)
									+ ")";
							NPCcant2[j] = 1;
						}
					}
				}
			}
		}

		WriteConsoleMsg(UserIndex, "Hostile NPCs in map: ", FontTypeNames_FONTTYPE_WARNING);
		if (NPCcount1 == 0) {
			WriteConsoleMsg(UserIndex, "There are no more hostile NPCs", FontTypeNames_FONTTYPE_INFO);
		} else {
			for (j = (0); j <= (NPCcount1 - 1); j++) {
				WriteConsoleMsg(UserIndex, vb6::CStr(NPCcant1[j]) + " " + vb6::CStr(List1[j]), FontTypeNames_FONTTYPE_INFO);
			}
		}
		WriteConsoleMsg(UserIndex, "Other NPCs in map: ", FontTypeNames_FONTTYPE_WARNING);
		if (NPCcount2 == 0) {
			WriteConsoleMsg(UserIndex, "No more NPCs.", FontTypeNames_FONTTYPE_INFO);
		} else {
			for (j = (0); j <= (NPCcount2 - 1); j++) {
				WriteConsoleMsg(UserIndex, vb6::CStr(NPCcant2[j]) + " " + vb6::CStr(List2[j]), FontTypeNames_FONTTYPE_INFO);
			}
		}
		LogGM(UserList[UserIndex].Name, "Number of enemies in map " + vb6::CStr(Map));
	}
}

/* '' */
/* ' Handles the "WarpMeToTarget" message. */
/* ' */


void HoAClientPacketHandler::handleWarpMeToTarget(WarpMeToTarget* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 26/03/09 */
	/* '26/03/06: ZaMa - Chequeo que no se teletransporte donde haya un char o npc */
	/* '*************************************************** */



	int X;
	int Y;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	X = UserList[UserIndex].flags.TargetX;
	Y = UserList[UserIndex].flags.TargetY;

	FindLegalPos(UserIndex, UserList[UserIndex].flags.TargetMap, X, Y);
	WarpUserChar(UserIndex, UserList[UserIndex].flags.TargetMap, X, Y, true);
	LogGM(UserList[UserIndex].Name,
			"/TELEPLOC a x:" + vb6::CStr(UserList[UserIndex].flags.TargetX) + " Y:" + vb6::CStr(UserList[UserIndex].flags.TargetY)
					+ " Map:" + vb6::CStr(UserList[UserIndex].Pos.Map));
}

/* '' */
/* ' Handles the "WarpChar" message. */
/* ' */


void HoAClientPacketHandler::handleWarpChar(WarpChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 26/03/2009 */
	/* '26/03/2009: ZaMa -  Chequeo que no se teletransporte a un tile donde haya un char o npc. */
	/* '*************************************************** */

	std::string UserName;
	int Map;
	int X;
	int Y;
	int tUser = 0;

	UserName = p->UserName;
	Map = p->Map;
	X = p->X;
	Y = p->Y;

	UserName = vb6::Replace(UserName, "+", " ");

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		if (MapaValido(Map) && vb6::LenB(UserName) != 0) {
			if (vb6::UCase(UserName) != "YO") {
				if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
					tUser = NameIndex(UserName);
				}
			} else {
				tUser = UserIndex;
			}

			if (tUser <= 0) {
				if (!(EsDios(UserName) || EsAdmin(UserName))) {
					WriteConsoleMsg(UserIndex, "User is offline", FontTypeNames_FONTTYPE_INFO);
				} else {
					WriteConsoleMsg(UserIndex, "You can't teleport admins",
							FontTypeNames_FONTTYPE_INFO);
				}

			} else if (!((UserTieneAlgunPrivilegios(tUser, PlayerType_Dios)) || (UserTieneAlgunPrivilegios(tUser, PlayerType_Admin))) || tUser == UserIndex) {

				if (InMapBounds(Map, X, Y)) {
					FindLegalPos(tUser, Map, X, Y);
					WarpUserChar(tUser, Map, X, Y, true, true);
					WriteConsoleMsg(UserIndex, UserList[tUser].Name + " teleported.",
							FontTypeNames_FONTTYPE_INFO);
					LogGM(UserList[UserIndex].Name,
							"Teleported " + UserList[tUser].Name + " towards " + "Map" + vb6::CStr(Map) + " X:" + vb6::CStr(X)
									+ " Y:" + vb6::CStr(Y));
				}
			} else {
				WriteConsoleMsg(UserIndex, "You can't teleport admins",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "Silence" message. */
/* ' */


void HoAClientPacketHandler::handleSilence(Silence* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		tUser = NameIndex(UserName);

		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User is offline", FontTypeNames_FONTTYPE_INFO);
		} else {
			if (UserList[tUser].flags.Silenciado == 0) {
				UserList[tUser].flags.Silenciado = 1;
				WriteConsoleMsg(UserIndex, "User muted.", FontTypeNames_FONTTYPE_INFO);
				WriteShowMessageBox(tUser,
						"Dear PC, you've been muted by the admins. Your complaints will be ignored from now on. Use /GM to contact an admin.");
				LogGM(UserList[UserIndex].Name, "/silenciar " + UserList[tUser].Name);

				/* 'Flush the other user's buffer */
				FlushBuffer(tUser);
			} else {
				UserList[tUser].flags.Silenciado = 0;
				WriteConsoleMsg(UserIndex, "User unmuted.", FontTypeNames_FONTTYPE_INFO);
				LogGM(UserList[UserIndex].Name, "/DESsilenciar " + UserList[tUser].Name);
			}
		}
	}




}

/* '' */
/* ' Handles the "SOSShowList" message. */
/* ' */


void HoAClientPacketHandler::handleSOSShowList(SOSShowList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}
	WriteShowSOSForm(UserIndex);
}

/* '' */
/* ' Handles the "RequestPartyForm" message. */
/* ' */


void HoAClientPacketHandler::handleRequestPartyForm(RequestPartyForm* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Budi */
	/* 'Last Modification: 11/26/09 */
	/* ' */
	/* '*************************************************** */

	if (UserList[UserIndex].PartyIndex > 0) {
		WriteShowPartyForm(UserIndex);

	} else {
		WriteConsoleMsg(UserIndex, "You are not part of a party!", FontTypeNames_FONTTYPE_INFOBOLD);
	}
}

/* '' */
/* ' Handles the "ItemUpgrade" message. */
/* ' */


void HoAClientPacketHandler::handleItemUpgrade(ItemUpgrade* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Torres Patricio */
	/* 'Last Modification: 12/09/09 */
	/* ' */
	/* '*************************************************** */
	int ItemIndex;

	ItemIndex = p->ItemIndex;

	if (ItemIndex <= 0) {
		return;
	}
	if (!TieneObjetos(ItemIndex, 1, UserIndex)) {
		return;
	}

	if (!IntervaloPermiteTrabajar(UserIndex)) {
		return;
	}
	DoUpgrade(UserIndex, ItemIndex);
}

/* '' */
/* ' Handles the "SOSRemove" message. */
/* ' */


void HoAClientPacketHandler::handleSOSRemove(SOSRemove* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		auto it = std::find(Ayuda.begin(), Ayuda.end(), UserName);
		if (it != Ayuda.end()) {
			Ayuda.erase(it);
		}
	}




}

/* '' */
/* ' Handles the "GoToChar" message. */
/* ' */


void HoAClientPacketHandler::handleGoToChar(GoToChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 26/03/2009 */
	/* '26/03/2009: ZaMa -  Chequeo que no se teletransporte a un tile donde haya un char o npc. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	int X;
	int Y;

	UserName = p->UserName;
	tUser = NameIndex(UserName);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_SemiDios, PlayerType_Consejero)) {
		/* 'Si es dios o Admins no podemos salvo que nosotros también lo seamos */
		if ((!(EsDios(UserName) || EsAdmin(UserName)))
				|| (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)
						&& !UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster))) {
			if (tUser <= 0) {
				WriteConsoleMsg(UserIndex, "User is offline", FontTypeNames_FONTTYPE_INFO);
			} else {
				X = UserList[tUser].Pos.X;
				Y = UserList[tUser].Pos.Y + 1;
				FindLegalPos(UserIndex, UserList[tUser].Pos.Map, X, Y);

				WarpUserChar(UserIndex, UserList[tUser].Pos.Map, X, Y, true);

				if (UserList[UserIndex].flags.AdminInvisible == 0) {
					WriteConsoleMsg(tUser,
							UserList[UserIndex].Name + " has teleported to your position.",
							FontTypeNames_FONTTYPE_INFO);
					FlushBuffer(tUser);
				}

				LogGM(UserList[UserIndex].Name,
						"/IRA " + UserName + " Map:" + vb6::CStr(UserList[tUser].Pos.Map) + " X:"
								+ vb6::CStr(UserList[tUser].Pos.X) + " Y:" + vb6::CStr(UserList[tUser].Pos.Y));
			}
		}
	}




}

/* '' */
/* ' Handles the "Invisible" message. */
/* ' */


void HoAClientPacketHandler::handleInvisible(Invisible* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	DoAdminInvisible(UserIndex);
	LogGM(UserList[UserIndex].Name, "/INVISIBLE");
}

/* '' */
/* ' Handles the "GMPanel" message. */
/* ' */


void HoAClientPacketHandler::handleGMPanel(GMPanel* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	WriteShowGMPanelForm(UserIndex);
}

/* '' */
/* ' Handles the "GMPanel" message. */
/* ' */


void HoAClientPacketHandler::handleRequestUserList(RequestUserList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/09/07 */
	/* 'Last modified by: Lucas Tavolaro Ortiz (Tavo) */
	/* 'I haven`t found a solution to split, so i make an array of names */
	/* '*************************************************** */
	int i;
	std::vector<std::string> names;
	int Count;




	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_RoleMaster)) {
		return;
	}

	names.resize(0);
	names.resize(1 + LastUser);
	Count = 1;

	for (i = (1); i <= (LastUser); i++) {
		if ((vb6::LenB(UserList[i].Name) != 0)) {
			if (UserTieneAlgunPrivilegios(i, PlayerType_User)) {
				names[Count] = UserList[i].Name;
				Count = Count + 1;
			}
		}
	}

	if (Count > 1) {
		WriteUserNameList(UserIndex, names, Count - 1);
	}
}

/* '' */
/* ' Handles the "Working" message. */
/* ' */


void HoAClientPacketHandler::handleWorking(Working* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 07/10/2010 */
	/* '07/10/2010: ZaMa - Adaptado para que funcione mas de un centinela en paralelo. */
	/* '*************************************************** */
	int i;
	std::string users;




	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_RoleMaster)) {
		return;
	}

	for (i = (1); i <= (LastUser); i++) {
		if (UserList[i].flags.UserLogged && UserList[i].Counters.Trabajando > 0) {
			users = users + ", " + UserList[i].Name;

			/* ' Display the user being checked by the centinel */
			if (UserList[i].flags.CentinelaIndex != 0) {
				users = users + " (*)";
			}
		}
	}

	if (vb6::LenB(users) != 0) {
		users = vb6::Right(users, vb6::Len(users) - 2);
		WriteConsoleMsg(UserIndex, "Users working: " + users, FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "There are no users working.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "Hiding" message. */
/* ' */


void HoAClientPacketHandler::handleHiding(Hiding* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 05/17/06 */
	/* ' */
	/* '*************************************************** */
	int i;
	std::string users;




	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_RoleMaster)) {
		return;
	}

	for (i = (1); i <= (LastUser); i++) {
		if ((vb6::LenB(UserList[i].Name) != 0) && UserList[i].Counters.Ocultando > 0) {
			users = users + UserList[i].Name + ", ";
		}
	}

	if (vb6::LenB(users) != 0) {
		users = vb6::Left(users, vb6::Len(users) - 2);
		WriteConsoleMsg(UserIndex, "Users hiding: " + users, FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "There are no users hiding.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "Jail" message. */
/* ' */


void HoAClientPacketHandler::handleJail(Jail* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */
	std::string UserName;
	std::string Reason;
	int jailTime;
	int Count;
	int tUser = 0;

	UserName = p->UserName;
	Reason = p->Reason;
	jailTime = p->JailTime;

	if (vb6::InStr(1, UserName, "+")) {
		UserName = vb6::Replace(UserName, "+", " ");
	}

	/* '/carcel nick@motivo@<tiempo> */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster, PlayerType_User)) {
		if (vb6::LenB(UserName) == 0 || vb6::LenB(Reason) == 0) {
			WriteConsoleMsg(UserIndex, "Use /jail nick@reason@time", FontTypeNames_FONTTYPE_INFO);
		} else {
			tUser = NameIndex(UserName);

			if (tUser <= 0) {
				if ((EsDios(UserName) || EsAdmin(UserName))) {
					WriteConsoleMsg(UserIndex, "You can't jail admins.",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					WriteConsoleMsg(UserIndex, "User is offline.", FontTypeNames_FONTTYPE_INFO);
				}
			} else {
				if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
					WriteConsoleMsg(UserIndex, "You can't jail admins.",
							FontTypeNames_FONTTYPE_INFO);
				} else if (jailTime > 60) {
					WriteConsoleMsg(UserIndex, "You can't jail someone for more than 60'.",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					if ((vb6::InStrB(UserName, "/") != 0)) {
						UserName = vb6::Replace(UserName, "/", "");
					}
					if ((vb6::InStrB(UserName, "/") != 0)) {
						UserName = vb6::Replace(UserName, "/", "");
					}

					auto userCharPath = GetCharPath(UserName);
					if (FileExist(userCharPath, 0)) {
						Count = vb6::val(GetVar(userCharPath, "PENAS", "Cant"));
						WriteVar(userCharPath, "PENAS", "Cant", Count + 1);
						WriteVar(userCharPath, "PENAS", "P" + vb6::CStr(Count + 1),
								vb6::LCase(UserList[UserIndex].Name) + ": JAIL " + vb6::CStr(jailTime)
										+ "m, REASON: " + vb6::LCase(Reason) + " " + vb6::dateToString(vb6::Now()));
					}

					Encarcelar(tUser, jailTime, UserList[UserIndex].Name);
					LogGM(UserList[UserIndex].Name, " jailed " + UserName);
				}
			}
		}
	}




}

/* '' */
/* ' Handles the "KillNPC" message. */
/* ' */


void HoAClientPacketHandler::handleKillNPC(KillNPC* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 04/22/08 (NicoNZ) */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User)) {
		return;
	}

	int tNPC;
	struct npc auxNPC;

	/* 'Los consejeros no pueden RMATAr a nada en el mapa pretoriano */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
//       if (UserList[UserIndex].Pos.Map == MAPA_PRETORIANO) {
//       WriteConsoleMsg(UserIndex, "Los consejeros no pueden usar este comando en el mapa pretoriano.", FontTypeNames_FONTTYPE_INFO);
//       return;
//      }
	}

	tNPC = UserList[UserIndex].flags.TargetNPC;

	if (tNPC > 0) {
		WriteConsoleMsg(UserIndex, "RMatas (with possible respawn) to: " + Npclist[tNPC].Name,
				FontTypeNames_FONTTYPE_INFO);

		auxNPC = Npclist[tNPC];
		QuitarNPC(tNPC);
		ReSpawnNpc(auxNPC);

		UserList[UserIndex].flags.TargetNPC = 0;
	} else {
		WriteConsoleMsg(UserIndex, "You must first click the NPC.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "WarnUser" message. */
/* ' */


void HoAClientPacketHandler::handleWarnUser(WarnUser* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/26/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string Reason;
	PlayerType Privs;
	int Count;

	UserName = p->UserName;
	Reason = p->Reason;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster, PlayerType_User)) {
		if (vb6::LenB(UserName) == 0 || vb6::LenB(Reason) == 0) {
			WriteConsoleMsg(UserIndex, "Use /Warning nick@reason", FontTypeNames_FONTTYPE_INFO);
		} else {
			Privs = UserDarPrivilegioLevel(UserName);

			if (!Privs && PlayerType_User) {
				WriteConsoleMsg(UserIndex, "You can't warn admins.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				if ((vb6::InStrB(UserName, "/") != 0)) {
					UserName = vb6::Replace(UserName, "/", "");
				}
				if ((vb6::InStrB(UserName, "/") != 0)) {
					UserName = vb6::Replace(UserName, "/", "");
				}

				if (FileExist(GetCharPath(UserName), 0)) {
					Count = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
					WriteVar(GetCharPath(UserName), "PENAS", "Cant", Count + 1);
					WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(Count + 1),
							vb6::LCase(UserList[UserIndex].Name) + ": ADVERTENCIA por: " + vb6::LCase(Reason)
									+ " " + vb6::dateToString(vb6::Now()));

					WriteConsoleMsg(UserIndex, "You've warned " + vb6::UCase(UserName) + ".",
							FontTypeNames_FONTTYPE_INFO);
					LogGM(UserList[UserIndex].Name, " warned " + UserName);
				}
			}
		}
	}



}

/* '' */
/* ' Handles the "EditChar" message. */
/* ' */


void HoAClientPacketHandler::handleEditChar(EditChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 18/09/2010 */
	/* '02/03/2009: ZaMa - Cuando editas nivel, chequea si el pj puede permanecer en clan faccionario */
	/* '11/06/2009: ZaMa - Todos los comandos se pueden usar aunque el pj este offline */
	/* '18/09/2010: ZaMa - Ahora se puede editar la vida del propio pj (cualquier rm o dios). */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	int opcion;
	std::string Arg1;
	std::string Arg2;
	bool valido = false;
	int LoopC;
	std::string CommandString;
	std::string UserCharPath;
	int Var;

	UserName = vb6::Replace(p->UserName, "+", " ");

	if (vb6::UCase(UserName) == "YO") {
		tUser = UserIndex;
	} else {
		tUser = NameIndex(UserName);
	}

	opcion = p->Opcion;
	Arg1 = p->Arg1;
	Arg2 = p->Arg2;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)) {
		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
			/* ' Los RMs consejeros sólo se pueden editar su head, body, level y vida */
			valido = tUser == UserIndex
					&& (opcion == eEditOptions_eo_Body || opcion == eEditOptions_eo_Head
							|| opcion == eEditOptions_eo_Level || opcion == eEditOptions_eo_Vida);

		} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios)) {
			/* ' Los RMs sólo se pueden editar su level o vida y el head y body de cualquiera */
			valido = ((opcion == eEditOptions_eo_Level || opcion == eEditOptions_eo_Vida)
					&& tUser == UserIndex) || opcion == eEditOptions_eo_Body
					|| opcion == eEditOptions_eo_Head;

		} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios)) {
			/* ' Los DRMs pueden aplicar los siguientes comandos sobre cualquiera */
			/* ' pero si quiere modificar el level o vida sólo lo puede hacer sobre sí mismo */
			valido = ((opcion == eEditOptions_eo_Level || opcion == eEditOptions_eo_Vida)
					&& tUser == UserIndex) || opcion == eEditOptions_eo_Body || opcion == eEditOptions_eo_Head
					|| opcion == eEditOptions_eo_CiticensKilled || opcion == eEditOptions_eo_CriminalsKilled
					|| opcion == eEditOptions_eo_Class || opcion == eEditOptions_eo_Skills
					|| opcion == eEditOptions_eo_addGold;
		}

		/* 'Si no es RM debe ser dios para poder usar este comando */
	} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {

		if (opcion == eEditOptions_eo_Vida) {
			/* '  Por ahora dejo para que los dioses no puedan editar la vida de otros */
			valido = (tUser == UserIndex);
		} else {
			valido = true;
		}

	} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios)) {

		valido =
				(opcion == eEditOptions_eo_Poss || ((opcion == eEditOptions_eo_Vida) && (tUser == UserIndex)));

		if (UserList[UserIndex].flags.PrivEspecial) {
			valido = valido || (opcion == eEditOptions_eo_CiticensKilled)
					|| (opcion == eEditOptions_eo_CriminalsKilled);
		}

	} else if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Consejero)) {
		valido = ((opcion == eEditOptions_eo_Vida) && (tUser == UserIndex));
	}

	if (valido) {
		UserCharPath = GetCharPath(UserName);
		if (tUser <= 0 && !FileExist(UserCharPath)) {
			WriteConsoleMsg(UserIndex, "Estás intentando editar un usuario inexistente.",
					FontTypeNames_FONTTYPE_INFO);
			LogGM(UserList[UserIndex].Name, "Intentó editar un usuario inexistente.");
		} else {
			/* 'For making the Log */
			CommandString = "/MOD ";

			switch (opcion) {
			case eEditOptions_eo_Gold:
				if (vb6::val(Arg1) <= MAX_ORO_EDIT) {
					/* ' Esta offline? */
					if (tUser <= 0) {
						WriteVar(UserCharPath, "STATS", "GLD", vb6::val(Arg1));
						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
						/* ' Online */
					} else {
						UserList[tUser].Stats.GLD = vb6::val(Arg1);
						WriteUpdateGold(tUser);
					}
				} else {
					WriteConsoleMsg(UserIndex,
							"No está permitido utilizar valores mayores a " + vb6::CStr(MAX_ORO_EDIT)
									+ ". Su comando ha quedado en los logs del juego.",
							FontTypeNames_FONTTYPE_INFO);
				}

				/* ' Log it */
				CommandString = CommandString + "ORO ";

				break;

			case eEditOptions_eo_Experience:
			{
				int Arg1int = vb6::val(Arg1);
				if (Arg1int > 20000000) {
					Arg1int = 20000000;
				}

				/* ' Offline */
				if (tUser <= 0) {
					Var = vb6::CInt(GetVar(UserCharPath, "STATS", "EXP"));
					WriteVar(UserCharPath, "STATS", "EXP", Var + Arg1int);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Stats.Exp = UserList[tUser].Stats.Exp + (Arg1int);
					CheckUserLevel(tUser);
					WriteUpdateExp(tUser);
				}

				/* ' Log it */
				CommandString = CommandString + "EXP ";
			}
				break;

			case eEditOptions_eo_Body:
				if (tUser <= 0) {
					WriteVar(UserCharPath, "INIT", "Body", Arg1);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
				} else {
					ChangeUserChar(tUser, vb6::val(Arg1), UserList[tUser].Char.Head,
							UserList[tUser].Char.heading, UserList[tUser].Char.WeaponAnim,
							UserList[tUser].Char.ShieldAnim, UserList[tUser].Char.CascoAnim);
				}

				/* ' Log it */
				CommandString = CommandString + "BODY ";

				break;

			case eEditOptions_eo_Head:
				if (tUser <= 0) {
					WriteVar(UserCharPath, "INIT", "Head", Arg1);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
				} else {
					ChangeUserChar(tUser, UserList[tUser].Char.body, vb6::val(Arg1),
							UserList[tUser].Char.heading, UserList[tUser].Char.WeaponAnim,
							UserList[tUser].Char.ShieldAnim, UserList[tUser].Char.CascoAnim);
				}

				/* ' Log it */
				CommandString = CommandString + "HEAD ";

				break;

			case eEditOptions_eo_CriminalsKilled:
				Var = vb6::IIf(vb6::val(Arg1) > MAXUSERMATADOS, (double)MAXUSERMATADOS, vb6::val(Arg1));

				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "FACCIONES", "CrimMatados", Var);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Faccion.CriminalesMatados = Var;
				}

				/* ' Log it */
				CommandString = CommandString + "CRI ";

				break;

			case eEditOptions_eo_CiticensKilled:
				Var = vb6::IIf(vb6::val(Arg1) > MAXUSERMATADOS, (double)MAXUSERMATADOS, vb6::val(Arg1));

				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "FACCIONES", "CiudMatados", Var);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Faccion.CiudadanosMatados = Var;
				}

				/* ' Log it */
				CommandString = CommandString + "CIU ";

				break;

			case eEditOptions_eo_Level:
				if (vb6::val(Arg1) > STAT_MAXELV) {
					Arg1 = vb6::CStr(STAT_MAXELV);
					WriteConsoleMsg(UserIndex, "No puedes tener un nivel superior a " + vb6::CStr(STAT_MAXELV) + ".",
							FontTypeNames_FONTTYPE_INFO);
				}

				/* ' Chequeamos si puede permanecer en el clan */
				if (vb6::val(Arg1) >= 25) {

					int GI;
					if (tUser <= 0) {
						GI = vb6::CInt(GetVar(UserCharPath, "GUILD", "GUILDINDEX"));
					} else {
						GI = UserList[tUser].GuildIndex;
					}

					if (GI > 0) {
						if (GuildAlignment(GI) == "Del Mal" || GuildAlignment(GI) == "Real") {
							/* 'We get here, so guild has factionary alignment, we have to expulse the user */
							m_EcharMiembroDeClan(-1, UserName);

							SendData(SendTarget_ToGuildMembers, GI,
									hoa::protocol::server::BuildConsoleMsg(UserName + " deja el clan.",
											FontTypeNames_FONTTYPE_GUILD));
							/* ' Si esta online le avisamos */
							if (tUser > 0) {
								WriteConsoleMsg(tUser,
										"¡Ya tienes la madurez suficiente como para decidir bajo que estandarte pelearás! Por esta razón, hasta tanto no te enlistes en la facción bajo la cual tu clan está alineado, estarás excluído del mismo.",
										FontTypeNames_FONTTYPE_GUILD);
							}
						}
					}
				}

				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "STATS", "ELV", vb6::val(Arg1));
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Stats.ELV = vb6::val(Arg1);
					WriteUpdateUserStats(tUser);
				}

				/* ' Log it */
				CommandString = CommandString + "LEVEL ";

				break;

			case eEditOptions_eo_Class:
				for (LoopC = (1); LoopC <= (NUMCLASES); LoopC++) {
					if (vb6::UCase(ListaClases[LoopC]) == vb6::UCase(Arg1)) {
						break;
					}
				}

				if (LoopC > NUMCLASES) {
					WriteConsoleMsg(UserIndex, "Clase desconocida. Intente nuevamente.",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					/* ' Offline */
					if (tUser <= 0) {
						WriteVar(UserCharPath, "INIT", "Clase", LoopC);
						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
						/* ' Online */
					} else {
						UserList[tUser].clase = static_cast<eClass>(LoopC);
					}
				}

				/* ' Log it */
				CommandString = CommandString + "CLASE ";

				break;

			case eEditOptions_eo_Skills:
				for (LoopC = (1); LoopC <= (NUMSKILLS); LoopC++) {
					if (vb6::UCase(vb6::Replace(SkillsNames[LoopC], " ", "+")) == vb6::UCase(Arg1)) {
						break;
					}
				}

				if (LoopC > NUMSKILLS) {
					WriteConsoleMsg(UserIndex, "Skill Inexistente!", FontTypeNames_FONTTYPE_INFO);
				} else {
					/* ' Offline */
					if (tUser <= 0) {
						WriteVar(UserCharPath, "Skills", "SK" + vb6::CStr(LoopC), Arg2);
						WriteVar(UserCharPath, "Skills", "EXPSK" + vb6::CStr(LoopC), 0);

						if (vb6::CInt(Arg2) < MAXSKILLPOINTS) {
							WriteVar(UserCharPath, "Skills", "ELUSK" + vb6::CStr(LoopC),
									ELU_SKILL_INICIAL * std::pow(1.05, vb6::CInt(Arg2)));
						} else {
							WriteVar(UserCharPath, "Skills", "ELUSK" + vb6::CStr(LoopC), 0);
						}

						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
						/* ' Online */
					} else {
						UserList[tUser].Stats.UserSkills[LoopC] = vb6::val(Arg2);
						CheckEluSkill(tUser, LoopC, true);
					}
				}

				/* ' Log it */
				CommandString = CommandString + "SKILLS ";

				break;

			case eEditOptions_eo_SkillPointsLeft:
				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "STATS", "SkillPtsLibres", Arg1);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Stats.SkillPts = vb6::val(Arg1);
				}

				/* ' Log it */
				CommandString = CommandString + "SKILLSLIBRES ";

				break;

			case eEditOptions_eo_Nobleza:
				Var = vb6::IIf(vb6::val(Arg1) > MAXREP, (double)MAXREP, vb6::val(Arg1));

				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "REP", "Nobles", Var);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Reputacion.NobleRep = Var;
				}

				/* ' Log it */
				CommandString = CommandString + "NOB ";

				break;

			case eEditOptions_eo_Asesino:
				Var = vb6::IIf(vb6::val(Arg1) > MAXREP, (double)MAXREP, vb6::val(Arg1));

				/* ' Offline */
				if (tUser <= 0) {
					WriteVar(UserCharPath, "REP", "Asesino", Var);
					WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName, FontTypeNames_FONTTYPE_INFO);
					/* ' Online */
				} else {
					UserList[tUser].Reputacion.AsesinoRep = Var;
				}

				/* ' Log it */
				CommandString = CommandString + "ASE ";

				break;

			case eEditOptions_eo_Sex:
				int Sex;
				/* ' Mujer? */
				Sex = vb6::IIf(vb6::UCase(Arg1) == "MUJER", (int) eGenero_Mujer, 0);
				/* ' Hombre? */
				Sex = vb6::IIf(vb6::UCase(Arg1) == "HOMBRE", (int) eGenero_Hombre, Sex);

				/* ' Es Hombre o mujer? */
				if (Sex != 0) {
					/* ' OffLine */
					if (tUser <= 0) {
						WriteVar(UserCharPath, "INIT", "Genero", Sex);
						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
						/* ' Online */
					} else {
						UserList[tUser].Genero = static_cast<eGenero>(Sex);
					}
				} else {
					WriteConsoleMsg(UserIndex, "Genero desconocido. Intente nuevamente.",
							FontTypeNames_FONTTYPE_INFO);
				}

				/* ' Log it */
				CommandString = CommandString + "SEX ";

				break;

			case eEditOptions_eo_Raza:
				int raza;

				Arg1 = vb6::UCase(Arg1);
				if (Arg1 == "HUMANO") {
					raza = eRaza_Humano;
				} else if (Arg1 == "ELFO") {
					raza = eRaza_Elfo;
				} else if (Arg1 == "DROW") {
					raza = eRaza_Drow;
				} else if (Arg1 == "ENANO") {
					raza = eRaza_Enano;
				} else if (Arg1 == "GNOMO") {
					raza = eRaza_Gnomo;
				} else {
					raza = 0;
				}

				if (raza == 0) {
					WriteConsoleMsg(UserIndex, "Raza desconocida. Intente nuevamente.",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					if (tUser <= 0) {
						WriteVar(UserCharPath, "INIT", "Raza", raza);
						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
					} else {
						UserList[tUser].raza = static_cast<eRaza>(raza);
					}
				}

				/* ' Log it */
				CommandString = CommandString + "RAZA ";
				break;

			case eEditOptions_eo_addGold:

				int bankGold;

				if (vb6::Abs(vb6::CInt(Arg1)) > MAX_ORO_EDIT) {
					WriteConsoleMsg(UserIndex,
							"No está permitido utilizar valores mayores a " + vb6::CStr(MAX_ORO_EDIT) + ".",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					if (tUser <= 0) {
						bankGold = vb6::CInt(GetVar(GetCharPath(UserName), "STATS", "BANCO"));
						WriteVar(UserCharPath, "STATS", "BANCO",
								vb6::IIf(bankGold + vb6::val(Arg1) <= 0, 0.0, bankGold + vb6::val(Arg1)));
						WriteConsoleMsg(UserIndex,
								"Se le ha agregado " + Arg1 + " monedas de oro a " + UserName + ".",
								FontTypeNames_FONTTYPE_TALK);
					} else {
						UserList[tUser].Stats.Banco = vb6::IIf(
								UserList[tUser].Stats.Banco + vb6::val(Arg1) <= 0, 0.0,
								UserList[tUser].Stats.Banco + vb6::val(Arg1));
						WriteConsoleMsg(tUser, STANDARD_BOUNTY_HUNTER_MESSAGE, FontTypeNames_FONTTYPE_TALK);
					}
				}

				/* ' Log it */
				CommandString = CommandString + "AGREGAR ";

				break;

			case eEditOptions_eo_Vida:

				if (vb6::val(Arg1) > MAX_VIDA_EDIT) {
					Arg1 = vb6::CStr(MAX_VIDA_EDIT);
					WriteConsoleMsg(UserIndex, "No puedes tener vida superior a " + vb6::CStr(MAX_VIDA_EDIT) + ".",
							FontTypeNames_FONTTYPE_INFO);
				}

				/* ' No valido si esta offline, porque solo se puede editar a si mismo */
				UserList[tUser].Stats.MaxHp = vb6::val(Arg1);
				UserList[tUser].Stats.MinHp = vb6::val(Arg1);

				WriteUpdateUserStats(tUser);

				/* ' Log it */
				CommandString = CommandString + "VIDA ";

				break;

			case eEditOptions_eo_Poss:

				int Map;
				int X;
				int Y;

				Map = vb6::val(ReadField(1, Arg1, 45));
				X = vb6::val(ReadField(2, Arg1, 45));
				Y = vb6::val(ReadField(3, Arg1, 45));

				if (InMapBounds(Map, X, Y)) {

					if (tUser <= 0) {
						WriteVar(UserCharPath, "INIT", "POSITION", vb6::string_format("%d-%d-%d", Map, X, Y));
						WriteConsoleMsg(UserIndex, "Charfile Alterado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
					} else {
						WarpUserChar(tUser, Map, X, Y, true, true);
						WriteConsoleMsg(UserIndex, "Usuario teletransportado: " + UserName,
								FontTypeNames_FONTTYPE_INFO);
					}
				} else {
					WriteConsoleMsg(UserIndex, "Posición inválida", FontTypeNames_FONTTYPE_INFO);
				}

				/* ' Log it */
				CommandString = CommandString + "POSS ";

				break;

			default:
				WriteConsoleMsg(UserIndex, "Comando no permitido.", FontTypeNames_FONTTYPE_INFO);
				CommandString = CommandString + "UNKOWN ";

				break;
			}

			CommandString = CommandString + Arg1 + " " + Arg2;
			LogGM(UserList[UserIndex].Name, CommandString + " " + UserName);

		}
	}




}

/* '' */
/* ' Handles the "RequestCharInfo" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharInfo(RequestCharInfo* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Fredy Horacio Treboux (liquid) */
	/* 'Last Modification: 01/08/07 */
	/* 'Last Modification by: (liquid).. alto bug zapallo.. */
	/* '*************************************************** */

	std::string TargetName;
	int TargetIndex;

	TargetName = vb6::Replace(p->TargetName, "+", " ");
	TargetIndex = NameIndex(TargetName);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		/* 'is the player offline? */
		if (TargetIndex <= 0) {
			/* 'don't allow to retrieve administrator's info */
			if (!(EsDios(TargetName) || EsAdmin(TargetName))) {
				WriteConsoleMsg(UserIndex, "Usuario offline, buscando en charfile.",
						FontTypeNames_FONTTYPE_INFO);
				SendUserStatsTxtOFF(UserIndex, TargetName);
			}
		} else {
			/* 'don't allow to retrieve administrator's info */
			if (UserTieneAlgunPrivilegios(TargetIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
				SendUserStatsTxt(UserIndex, TargetIndex);
			}
		}
	}




}

/* '' */
/* ' Handles the "RequestCharStats" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharStats(RequestCharStats* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	bool UserIsAdmin = false;
	bool OtherUserIsAdmin = false;

	UserName = p->UserName;

	UserIsAdmin = UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios);

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios) || UserIsAdmin)) {
		LogGM(UserList[UserIndex].Name, "/STAT " + UserName);

		tUser = NameIndex(UserName);

		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		if (tUser <= 0) {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex, "User is offline Leyendo charfile... ",
						FontTypeNames_FONTTYPE_INFO);

				SendUserMiniStatsTxtFromChar(UserIndex, UserName);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver los stats de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				SendUserMiniStatsTxt(UserIndex, tUser);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver los stats de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "RequestCharGold" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharGold(RequestCharGold* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	bool UserIsAdmin = false;
	bool OtherUserIsAdmin = false;

	UserName = p->UserName;

	UserIsAdmin = (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios));

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios) || UserIsAdmin) {

		LogGM(UserList[UserIndex].Name, "/BAL " + UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		if (tUser <= 0) {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex, "User is offline Leyendo charfile... ",
						FontTypeNames_FONTTYPE_TALK);

				SendUserOROTxtFromChar(UserIndex, UserName);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver el oro de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex,
						"El usuario " + UserName + " tiene " + vb6::CStr(UserList[tUser].Stats.Banco) + " en el banco.",
						FontTypeNames_FONTTYPE_TALK);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver el oro de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "RequestCharInventory" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharInventory(RequestCharInventory* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	bool UserIsAdmin = false;
	bool OtherUserIsAdmin = false;

	UserName = p->UserName;

	UserIsAdmin = (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios));

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		LogGM(UserList[UserIndex].Name, "/INV " + UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		if (tUser <= 0) {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex, "User is offline Leyendo del charfile...",
						FontTypeNames_FONTTYPE_TALK);

				SendUserInvTxtFromChar(UserIndex, UserName);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver el inventario de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				SendUserInvTxt(UserIndex, tUser);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver el inventario de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "RequestCharBank" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharBank(RequestCharBank* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	bool UserIsAdmin = false;
	bool OtherUserIsAdmin = false;

	UserName = p->UserName;

	UserIsAdmin = (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios));

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios) || UserIsAdmin) {
		LogGM(UserList[UserIndex].Name, "/BOV " + UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		tUser = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		if (tUser <= 0) {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex, "User is offline Leyendo charfile... ",
						FontTypeNames_FONTTYPE_TALK);

				SendUserBovedaTxtFromChar(UserIndex, UserName);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver la bóveda de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				SendUserBovedaTxt(UserIndex, tUser);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes ver la bóveda de un dios o admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "RequestCharSkills" message. */
/* ' */


void HoAClientPacketHandler::handleRequestCharSkills(RequestCharSkills* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	int LoopC;
	std::string message;

	UserName = p->UserName;
	tUser = NameIndex(UserName);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		LogGM(UserList[UserIndex].Name, "/STATS " + UserName);

		if (tUser <= 0) {
			if ((vb6::InStrB(UserName, "/") != 0)) {
				UserName = vb6::Replace(UserName, "/", "");
			}
			if ((vb6::InStrB(UserName, "/") != 0)) {
				UserName = vb6::Replace(UserName, "/", "");
			}

			for (LoopC = (1); LoopC <= (NUMSKILLS); LoopC++) {
				message = message + "CHAR>" + SkillsNames[LoopC] + " = "
						+ GetVar(GetCharPath(UserName), "SKILLS", "SK" + vb6::CStr(LoopC)) + vbCrLf;
			}

			WriteConsoleMsg(UserIndex,
					message + "CHAR> Libres:"
							+ GetVar(GetCharPath(UserName), "STATS", "SKILLPTSLIBRES"),
					FontTypeNames_FONTTYPE_INFO);
		} else {
			SendUserSkillsTxt(UserIndex, tUser);
		}
	}




}

/* '' */
/* ' Handles the "ReviveChar" message. */
/* ' */


void HoAClientPacketHandler::handleReviveChar(ReviveChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 11/03/2010 */
	/* '11/03/2010: ZaMa - Al revivir con el comando, si esta navegando le da cuerpo e barca. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		if (vb6::UCase(UserName) != "YO") {
			tUser = NameIndex(UserName);
		} else {
			tUser = UserIndex;
		}

		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User is offline", FontTypeNames_FONTTYPE_INFO);
		} else {
			/* 'If dead, show him alive (naked). */
			if (UserList[tUser].flags.Muerto == 1) {
				UserList[tUser].flags.Muerto = 0;

				if (UserList[tUser].flags.Navegando == 1) {
					ToggleBoatBody(tUser);
				} else {
					DarCuerpoDesnudo(tUser);
				}

				if (UserList[tUser].flags.Traveling == 1) {
					UserList[tUser].flags.Traveling = 0;
					UserList[tUser].Counters.goHome = 0;
					WriteMultiMessage(tUser, eMessages_CancelHome);
				}

				ChangeUserChar(tUser, UserList[tUser].Char.body, UserList[tUser].OrigChar.Head,
						UserList[tUser].Char.heading, UserList[tUser].Char.WeaponAnim,
						UserList[tUser].Char.ShieldAnim, UserList[tUser].Char.CascoAnim);

				WriteConsoleMsg(tUser, UserList[UserIndex].Name + " has revived you.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(tUser, UserList[UserIndex].Name + " has cured you.",
						FontTypeNames_FONTTYPE_INFO);
			}

			UserList[tUser].Stats.MinHp = UserList[tUser].Stats.MaxHp;

			if (UserList[tUser].flags.Traveling == 1) {
				UserList[tUser].Counters.goHome = 0;
				UserList[tUser].flags.Traveling = 0;
				WriteMultiMessage(tUser, eMessages_CancelHome);
			}

			WriteUpdateHP(tUser);

			FlushBuffer(tUser);

			LogGM(UserList[UserIndex].Name, "Revived " + UserName);
		}
	}




}

/* '' */
/* ' Handles the "OnlineGM" message. */
/* ' */


void HoAClientPacketHandler::handleOnlineGM(OnlineGM* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Fredy Horacio Treboux (liquid) */
	/* 'Last Modification: 12/28/06 */
	/* ' */
	/* '*************************************************** */
	int i;
	std::string list;
	bool esdios = false;




	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		esdios = true;
	}

	for (i = (1); i <= (LastUser); i++) {
		if (UserList[i].flags.UserLogged) {
			if (!UserTienePrivilegio(i, PlayerType_User)) {
				if (esdios || UserTieneMasPrivilegiosQue(UserIndex, i)) {
					list = list + UserList[i].Name + ", ";
				}
			}
		}
	}

	if (vb6::LenB(list) != 0) {
		list = vb6::Left(list, vb6::Len(list) - 2);
		WriteConsoleMsg(UserIndex, list + ".", FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "No online GMss.", FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "OnlineMap" message. */
/* ' */


void HoAClientPacketHandler::handleOnlineMap(OnlineMap* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 23/03/2009 */
	/* '23/03/2009: ZaMa - Ahora no requiere estar en el mapa, sino que por defecto se toma en el que esta, pero se puede especificar otro */
	/* '*************************************************** */



	int Map;
	Map = p->Map;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	int LoopC;
	std::string list;
	bool esdios = false;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		esdios = true;
	}

	for (LoopC = (1); LoopC <= (LastUser); LoopC++) {
		if (UserList[LoopC].flags.UserLogged && UserList[LoopC].Pos.Map == Map) {
			if (esdios || UserTieneAlgunPrivilegios(LoopC, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
				list = list + UserList[LoopC].Name + ", ";
			}
		}
	}

	if (vb6::Len(list) > 2) {
		list = vb6::Left(list, vb6::Len(list) - 2);
	}

	WriteConsoleMsg(UserIndex, "Users in map: " + list, FontTypeNames_FONTTYPE_INFO);
	LogGM(UserList[UserIndex].Name, "/ONLINEMAP " + vb6::CStr(Map));
}

/* '' */
/* ' Handles the "Forgive" message. */
/* ' */


void HoAClientPacketHandler::handleForgive(Forgive* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		tUser = NameIndex(UserName);

		if (tUser > 0) {
			if (EsNewbie(tUser)) {
				VolverCiudadano(tUser);
			} else {
				LogGM(UserList[UserIndex].Name, "Tried to pardon a non-newbie character.");

				if (!(EsDios(UserName) || EsAdmin(UserName))) {
					WriteConsoleMsg(UserIndex, "Only newbies can be pardoned.",
							FontTypeNames_FONTTYPE_INFO);
				}
			}
		}
	}
}

/* '' */
/* ' Handles the "Kick" message. */
/* ' */


void HoAClientPacketHandler::handleKick(Kick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	bool IsAdmin;

	UserName = p->UserName;
	IsAdmin = UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios) | IsAdmin) {
		tUser = NameIndex(UserName);

		if (tUser <= 0) {
			if (!(EsDios(UserName) || EsAdmin(UserName)) || IsAdmin) {
				WriteConsoleMsg(UserIndex, "User is offline.", FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, "No puedes echar a alguien con jerarquía mayor a la tuya.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserTieneMasPrivilegiosQue(tUser, UserIndex)) {
				WriteConsoleMsg(UserIndex, "No puedes echar a alguien con jerarquía mayor a la tuya.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				CloseSocket(tUser);
				LogGM(UserList[UserIndex].Name, "Echó a " + UserName);
			}
		}
	}




}

/* '' */
/* ' Handles the "Execute" message. */
/* ' */


void HoAClientPacketHandler::handleExecute(Execute* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		tUser = NameIndex(UserName);

		if (tUser > 0) {
			if (!UserTieneAlgunPrivilegios(tUser, PlayerType_User)) {
				WriteConsoleMsg(UserIndex, "You can't execute admins.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				UserDie(tUser);
				SendData(SendTarget_ToAll, 0,
						hoa::protocol::server::BuildConsoleMsg(
								UserList[UserIndex].Name + " has executed " + UserName + ".",
								FontTypeNames_FONTTYPE_EJECUCION));
				LogGM(UserList[UserIndex].Name, " executed " + UserName);
			}
		} else {
			if (!(EsDios(UserName) || EsAdmin(UserName))) {
				WriteConsoleMsg(UserIndex, "They're offline.", FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, "You can't execute admins.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "BanChar" message. */
/* ' */


void HoAClientPacketHandler::handleBanChar(BanChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string Reason;

	UserName = p->UserName;
	Reason = p->Reason;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		BanCharacter(UserIndex, UserName, Reason);
	}
}

/* '' */
/* ' Handles the "UnbanChar" message. */
/* ' */


void HoAClientPacketHandler::handleUnbanChar(UnbanChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int cantPenas;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}

		if (!FileExist(GetCharPath(UserName), 0)) {
			WriteConsoleMsg(UserIndex, "Charfile inexistente (no use +).", FontTypeNames_FONTTYPE_INFO);
		} else {
			if ((vb6::val(GetVar(GetCharPath(UserName), "FLAGS", "Ban")) == 1)) {
				UnBan(UserName);

				/* 'penas */
				cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
				WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
				WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
						vb6::LCase(UserList[UserIndex].Name) + ": UNBAN. " + vb6::dateToString(vb6::Now()));

				LogGM(UserList[UserIndex].Name, "/UNBAN a " + UserName);
				WriteConsoleMsg(UserIndex, UserName + " unbanned.", FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, UserName + " no está baneado. Imposible unbanear.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "NPCFollow" message. */
/* ' */


void HoAClientPacketHandler::handleNPCFollow(NPCFollow* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	if (UserList[UserIndex].flags.TargetNPC > 0) {
		DoFollow(UserList[UserIndex].flags.TargetNPC, UserList[UserIndex].Name);
		Npclist[UserList[UserIndex].flags.TargetNPC].flags.Inmovilizado = 0;
		Npclist[UserList[UserIndex].flags.TargetNPC].flags.Paralizado = 0;
		Npclist[UserList[UserIndex].flags.TargetNPC].Contadores.Paralisis = 0;
	}
}

/* '' */
/* ' Handles the "SummonChar" message. */
/* ' */


void HoAClientPacketHandler::handleSummonChar(SummonChar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 26/03/2009 */
	/* '26/03/2009: ZaMa - Chequeo que no se teletransporte donde haya un char o npc */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	int X;
	int Y;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		tUser = NameIndex(UserName);

		if (tUser <= 0) {
			if (EsDios(UserName) || EsAdmin(UserName)) {
				WriteConsoleMsg(UserIndex, "You can't summon admins.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, "Player is offline.", FontTypeNames_FONTTYPE_INFO);
			}

		} else {
			if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)
					|| UserTieneAlgunPrivilegios(tUser, PlayerType_Consejero, PlayerType_User)) {
				WriteConsoleMsg(tUser, UserList[UserIndex].Name + " has teleported you.",
						FontTypeNames_FONTTYPE_INFO);
				X = UserList[UserIndex].Pos.X;
				Y = UserList[UserIndex].Pos.Y + 1;
				FindLegalPos(tUser, UserList[UserIndex].Pos.Map, X, Y);
				WarpUserChar(tUser, UserList[UserIndex].Pos.Map, X, Y, true, true);
				LogGM(UserList[UserIndex].Name,
						 vb6::string_format("/SUM %s Map: %d X: %d Y: %d",
								UserName.c_str(),
								UserList[UserIndex].Pos.Map,
								UserList[UserIndex].Pos.X,
								UserList[UserIndex].Pos.Y));
			} else {
				WriteConsoleMsg(UserIndex, "You can't summon admins.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}
}

/* '' */
/* ' Handles the "SpawnListRequest" message. */
/* ' */


void HoAClientPacketHandler::handleSpawnListRequest(SpawnListRequest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	EnviarSpawnList(UserIndex);
}

/* '' */
/* ' Handles the "SpawnCreature" message. */
/* ' */


void HoAClientPacketHandler::handleSpawnCreature(SpawnCreature* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	int npc;
	npc = p->NPC;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		if (npc > 0 && npc <= vb6::UBound(SpawnList)) {
			SpawnNpc(SpawnList[npc].NpcIndex, UserList[UserIndex].Pos, true, false);
		}

		LogGM(UserList[UserIndex].Name, "Sumoneo " + SpawnList[npc].NpcName);
	}
}

/* '' */
/* ' Handles the "ResetNPCInventory" message. */
/* ' */


void HoAClientPacketHandler::handleResetNPCInventory(ResetNPCInventory* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	if (UserList[UserIndex].flags.TargetNPC == 0) {
		return;
	}

	ResetNpcInv(UserList[UserIndex].flags.TargetNPC);
	LogGM(UserList[UserIndex].Name, "/RESETINV " + Npclist[UserList[UserIndex].flags.TargetNPC].Name);
}

/* '' */
/* ' Handles the "CleanWorld" message. */
/* ' */


void HoAClientPacketHandler::handleCleanWorld(CleanWorld* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	LimpiarMundo();
}

/* '' */
/* ' Handles the "ServerMessage" message. */
/* ' */


void HoAClientPacketHandler::handleServerMessage(ServerMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 28/05/2010 */
	/* '28/05/2010: ZaMa - Ahora no dice el nombre del gm que lo dice. */
	/* '*************************************************** */

	std::string& message = p->Message;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		if (vb6::LenB(message) != 0) {
			LogGM(UserList[UserIndex].Name, "Mensaje Broadcast:" + message);
			SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildConsoleMsg(message, FontTypeNames_FONTTYPE_TALK));
			/* ''''''''''''''''SOLO PARA EL TESTEO''''''' */
			/* ''''''''''SE USA PARA COMUNICARSE CON EL SERVER''''''''''' */
			/* 'frmMain.txtChat.Text = frmMain.txtChat.Text & vbNewLine & UserList(UserIndex).name & " > " & message */
		}
	}
}

/* '' */
/* ' Handles the "MapMessage" message. */
/* ' */


void HoAClientPacketHandler::handleMapMessage(MapMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/11/2010 */
	/* '*************************************************** */
	std::string& message = p->Message;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)
			|| (UserTienePrivilegio(UserIndex, PlayerType_Consejero)
					&& UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {

		if (vb6::LenB(message) != 0) {

			int mapa;
			mapa = UserList[UserIndex].Pos.Map;

			LogGM(UserList[UserIndex].Name, "Mensaje a mapa " + vb6::CStr(mapa) + ":" + message);
			SendData(SendTarget_toMap, mapa, hoa::protocol::server::BuildConsoleMsg(message, FontTypeNames_FONTTYPE_TALK));
		}
	}




}

/* '' */
/* ' Handles the "NickToIP" message. */
/* ' */


void HoAClientPacketHandler::handleNickToIP(NickToIP* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 07/06/2010 */
	/* 'Pablo (ToxicWaste): Agrego para que el /nick2ip tambien diga los nicks en esa ip por pedido de la DGM. */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;
	bool IsAdmin = false;

	UserName = p->UserName;

	if (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		tUser = NameIndex(UserName);
		LogGM(UserList[UserIndex].Name, "NICK2IP Solicito la IP de " + UserName);

		IsAdmin = UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin);

		if (tUser > 0) {
			bool valid = false;

			if (IsAdmin) {
				valid = UserTieneAlgunPrivilegios(tUser, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_Dios, PlayerType_Admin);
			} else {
				valid = UserTieneAlgunPrivilegios(tUser, PlayerType_User);
			}

			if (valid) {
				WriteConsoleMsg(UserIndex, "El ip de " + UserName + " es " + UserList[tUser].ip,
						FontTypeNames_FONTTYPE_INFO);
				std::string ip;
				std::string lista;
				int LoopC;
				ip = UserList[tUser].ip;
				for (LoopC = (1); LoopC <= (LastUser); LoopC++) {
					if (UserList[LoopC].ip == ip) {
						if (vb6::LenB(UserList[LoopC].Name) != 0 && UserList[LoopC].flags.UserLogged) {
							if (IsAdmin || UserTienePrivilegio(LoopC, PlayerType_User)) {
								lista = lista + UserList[LoopC].Name + ", ";
							}
						}
					}
				}
				if (vb6::LenB(lista) != 0) {
					lista = vb6::Left(lista, vb6::Len(lista) - 2);
				}
				WriteConsoleMsg(UserIndex, "Los personajes con ip " + ip + " son: " + lista,
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (!(EsDios(UserName) || EsAdmin(UserName)) || IsAdmin) {
				WriteConsoleMsg(UserIndex, "No hay ningún personaje con ese nick.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "IPToNick" message. */
/* ' */


void HoAClientPacketHandler::handleIPToNick(IPToNick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string ip;
	int LoopC;
	std::string lista;

	ip = vb6::string_format("%d.%d.%d.%d", p->A, p->B, p->C, p->D);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, "IP2NICK Solicito los Nicks de IP " + ip);
	bool IsAdmin;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		IsAdmin = true;
	} else {
		IsAdmin = false;
	}

	for (LoopC = (1); LoopC <= (LastUser); LoopC++) {
		if (UserList[LoopC].ip == ip) {
			if (vb6::LenB(UserList[LoopC].Name) != 0 && UserList[LoopC].flags.UserLogged) {
				if (IsAdmin || UserTienePrivilegio(LoopC, PlayerType_User)) {
					lista = lista + UserList[LoopC].Name + ", ";
				}
			}
		}
	}

	if (vb6::LenB(lista) != 0) {
		lista = vb6::Left(lista, vb6::Len(lista) - 2);
	}
	WriteConsoleMsg(UserIndex, "Los personajes con ip " + ip + " son: " + lista, FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "GuildOnlineMembers" message. */
/* ' */


void HoAClientPacketHandler::handleGuildOnlineMembers(GuildOnlineMembers* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string GuildName;
	int tGuild;

	GuildName = p->GuildName;

	if ((vb6::InStrB(GuildName, "+") != 0)) {
		GuildName = vb6::Replace(GuildName, "+", " ");
	}

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		tGuild = GetGuildIndex(GuildName);

		if (tGuild > 0) {
			WriteConsoleMsg(UserIndex,
					"Clan " + vb6::UCase(GuildName) + ": " + m_ListaDeMiembrosOnline(UserIndex, tGuild),
					FontTypeNames_FONTTYPE_GUILDMSG);
		}
	}
}

/* '' */
/* ' Handles the "TeleportCreate" message. */
/* ' */


void HoAClientPacketHandler::handleTeleportCreate(TeleportCreate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 22/03/2010 */
	/* '15/11/2009: ZaMa - Ahora se crea un teleport con un radio especificado. */
	/* '22/03/2010: ZaMa - Harcodeo los teleps y radios en el dat, para evitar mapas bugueados. */
	/* '*************************************************** */

	int mapa;
	int X;
	int Y;
	int Radio;

	mapa = p->Map;
	X = p->X;
	Y = p->Y;
	Radio = p->Radio;

	Radio = MinimoInt(Radio, 6);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, vb6::string_format("/CT %d,%d,%d,%d", mapa, X, Y, Radio));

	if (!MapaValido(mapa) || !InMapBounds(mapa, X, Y)) {
		return;
	}

	if (MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y - 1].ObjInfo.ObjIndex
			> 0) {
		return;
	}

	if (MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y - 1].TileExit.Map
			> 0) {
		return;
	}

	if (MapData[mapa][X][Y].ObjInfo.ObjIndex > 0) {
		WriteConsoleMsg(UserIndex, "There's already an object there.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	if (MapData[mapa][X][Y].TileExit.Map > 0) {
		WriteConsoleMsg(UserIndex, "You can't create a teleport that points to another one.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	struct Obj ET;
	ET.Amount = 1;
	/* ' Es el numero en el dat. El indice es el comienzo + el radio, todo harcodeado :(. */
	ET.ObjIndex = TELEP_OBJ_INDEX + Radio;

	MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y - 1].TileExit.Map =
			mapa;
	MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y - 1].TileExit.X =
			X;
	MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y - 1].TileExit.Y =
			Y;

	MakeObj(ET, UserList[UserIndex].Pos.Map, UserList[UserIndex].Pos.X, UserList[UserIndex].Pos.Y - 1);
}

/* '' */
/* ' Handles the "TeleportDestroy" message. */
/* ' */


void HoAClientPacketHandler::handleTeleportDestroy(TeleportDestroy* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */
	int mapa;
	int X;
	int Y;




	/* '/dt */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	mapa = UserList[UserIndex].flags.TargetMap;
	X = UserList[UserIndex].flags.TargetX;
	Y = UserList[UserIndex].flags.TargetY;

	if (!InMapBounds(mapa, X, Y)) {
		return;
	}

	if (MapData[mapa][X][Y].ObjInfo.ObjIndex == 0) {
		return;
	}

	if (ObjData[MapData[mapa][X][Y].ObjInfo.ObjIndex].OBJType == eOBJType_otTeleport
			&& MapData[mapa][X][Y].TileExit.Map > 0) {
		LogGM(UserList[UserIndex].Name, vb6::string_format("/DT: %d,%d,%d", mapa, X, Y));

		EraseObj(MapData[mapa][X][Y].ObjInfo.Amount, mapa, X, Y);

		if (MapData[MapData[mapa][X][Y].TileExit.Map][MapData[mapa][X][Y].TileExit.X][MapData[mapa][X][Y].TileExit.Y].ObjInfo.ObjIndex
				== 651) {
			EraseObj(1, MapData[mapa][X][Y].TileExit.Map, MapData[mapa][X][Y].TileExit.X,
					MapData[mapa][X][Y].TileExit.Y);
		}

		MapData[mapa][X][Y].TileExit.Map = 0;
		MapData[mapa][X][Y].TileExit.X = 0;
		MapData[mapa][X][Y].TileExit.Y = 0;
	}
}

/* '' */
/* ' Handles the "RainToggle" message. */
/* ' */


void HoAClientPacketHandler::handleRainToggle(RainToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, "/LLUVIA");
	Lloviendo = !Lloviendo;

	SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildRainToggle());
}

/* '' */
/* ' Handles the "EnableDenounces" message. */
/* ' */


void HoAClientPacketHandler::handleEnableDenounces(EnableDenounces* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/11/2010 */
	/* 'Enables/Disables */
	/* '*************************************************** */

	/* ' Gm? */
	if (!EsGm(UserIndex)) {
		return;
	}
	/* ' Rm? */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)) {
		return;
	}

	bool Activado;
	std::string msg;

	Activado = !UserList[UserIndex].flags.SendDenounces;
	UserList[UserIndex].flags.SendDenounces = Activado;

	msg = std::string("Console complaints ") + vb6::IIf(Activado, "enabled", "disabled") + ".";

	LogGM(UserList[UserIndex].Name, msg);

	WriteConsoleMsg(UserIndex, msg, FontTypeNames_FONTTYPE_INFO);

}

/* '' */
/* ' Handles the "ShowDenouncesList" message. */
/* ' */


void HoAClientPacketHandler::handleShowDenouncesList(ShowDenouncesList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 14/11/2010 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_RoleMaster)) {
		return;
	}
	WriteShowDenounces(UserIndex);
}

/* '' */
/* ' Handles the "SetCharDescription" message. */
/* ' */


void HoAClientPacketHandler::handleSetCharDescription(SetCharDescription* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	int tUser = 0;
	std::string desc;

	desc = p->Description;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		tUser = UserList[UserIndex].flags.TargetUser;
		if (tUser > 0) {
			UserList[tUser].DescRM = desc;
		} else {
			WriteConsoleMsg(UserIndex, "You need to click a player character before.", FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handles the "ForceMIDIToMap" message. */
/* ' */


void HoAClientPacketHandler::handleForceMIDIToMap(ForceMIDIToMap* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	int midiID;
	int mapa;

	midiID = p->MidiID;
	mapa = p->Map;

	/* 'Solo dioses, admins y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		/* 'Si el mapa no fue enviado tomo el actual */
		if (!InMapBounds(mapa, 50, 50)) {
			mapa = UserList[UserIndex].Pos.Map;
		}

		if (midiID == 0) {
			/* 'Ponemos el default del mapa */
			SendData(SendTarget_toMap, mapa,
					hoa::protocol::server::BuildPlayMidi(MapInfo[UserList[UserIndex].Pos.Map].Music, 0));
		} else {
			/* 'Ponemos el pedido por el GM */
			SendData(SendTarget_toMap, mapa, hoa::protocol::server::BuildPlayMidi(midiID, 0));
		}
	}
}

/* '' */
/* ' Handles the "ForceWAVEToMap" message. */
/* ' */


void HoAClientPacketHandler::handleForceWAVEToMap(ForceWAVEToMap* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	int waveID;
	int mapa;
	int X;
	int Y;

	waveID = p->Wave;
	mapa = p->Map;
	X = p->X;
	Y = p->Y;

	/* 'Solo dioses, admins y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		/* 'Si el mapa no fue enviado tomo el actual */
		if (!InMapBounds(mapa, X, Y)) {
			mapa = UserList[UserIndex].Pos.Map;
			X = UserList[UserIndex].Pos.X;
			Y = UserList[UserIndex].Pos.Y;
		}

		/* 'Ponemos el pedido por el GM */
		SendData(SendTarget_toMap, mapa, hoa::protocol::server::BuildPlayWave(waveID, X, Y));
	}
}

/* '' */
/* ' Handles the "RoyalArmyMessage" message. */
/* ' */


void HoAClientPacketHandler::handleRoyalArmyMessage(RoyalArmyMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;

	/* 'Solo dioses, admins, semis y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		SendData(SendTarget_ToRealYRMs, 0,
				hoa::protocol::server::BuildConsoleMsg("ROYAL ARMY> " + message, FontTypeNames_FONTTYPE_TALK));
	}
}

/* '' */
/* ' Handles the "ChaosLegionMessage" message. */
/* ' */


void HoAClientPacketHandler::handleChaosLegionMessage(ChaosLegionMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;

	/* 'Solo dioses, admins, semis y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		SendData(SendTarget_ToCaosYRMs, 0,
				hoa::protocol::server::BuildConsoleMsg("DARK LEGION> " + message, FontTypeNames_FONTTYPE_TALK));
	}
}

/* '' */
/* ' Handles the "CitizenMessage" message. */
/* ' */


void HoAClientPacketHandler::handleCitizenMessage(CitizenMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;

	/* 'Solo dioses, admins, semis y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		SendData(SendTarget_ToCiudadanosYRMs, 0,
				hoa::protocol::server::BuildConsoleMsg("CITIZENS> " + message, FontTypeNames_FONTTYPE_TALK));
	}




}

/* '' */
/* ' Handles the "CriminalMessage" message. */
/* ' */


void HoAClientPacketHandler::handleCriminalMessage(CriminalMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;


	/* 'Solo dioses, admins y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		SendData(SendTarget_ToCriminalesYRMs, 0,
				hoa::protocol::server::BuildConsoleMsg("CRIMINALS> " + message, FontTypeNames_FONTTYPE_TALK));
	}




}

/* '' */
/* ' Handles the "TalkAsNPC" message. */
/* ' */


void HoAClientPacketHandler::handleTalkAsNPC(TalkAsNPC* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/29/06 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;

	/* 'Solo dioses, admins y RMS */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin, PlayerType_RoleMaster)) {
		/* 'Asegurarse haya un NPC seleccionado */
		if (UserList[UserIndex].flags.TargetNPC > 0) {
			SendData(SendTarget_ToNPCArea, UserList[UserIndex].flags.TargetNPC,
					BuildChatOverHead(message,
							Npclist[UserList[UserIndex].flags.TargetNPC].Char.CharIndex, 0x00ffffff));
		} else {
			WriteConsoleMsg(UserIndex,
					"You must first select the NPC you would like to speak through first.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}




}

/* '' */
/* ' Handles the "DestroyAllItemsInArea" message. */
/* ' */


void HoAClientPacketHandler::handleDestroyAllItemsInArea(DestroyAllItemsInArea* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	int X;
	int Y;
	bool bIsExit = false;

	for (Y = (UserList[UserIndex].Pos.Y - MinYBorder + 1); Y <= (UserList[UserIndex].Pos.Y + MinYBorder - 1);
			Y++) {
		for (X = (UserList[UserIndex].Pos.X - MinXBorder + 1);
				X <= (UserList[UserIndex].Pos.X + MinXBorder - 1); X++) {
			if (X > 0 && Y > 0 && X < 101 && Y < 101) {
				if (MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex > 0) {
					bIsExit = MapData[UserList[UserIndex].Pos.Map][X][Y].TileExit.Map > 0;
					if (ItemNoEsDeMapa(MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex,
							bIsExit)) {
						EraseObj(MAX_INVENTORY_OBJS, UserList[UserIndex].Pos.Map, X, Y);
					}
				}
			}
		}
	}

	LogGM(UserList[UserIndex].Name, "/MASSDEST");
}

/* '' */
/* ' Handles the "AcceptRoyalCouncilMember" message. */
/* ' */


void HoAClientPacketHandler::handleAcceptRoyalCouncilMember(AcceptRoyalCouncilMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		tUser = NameIndex(UserName);
		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User offline", FontTypeNames_FONTTYPE_INFO);
		} else {
			SendData(SendTarget_ToAll, 0,
					hoa::protocol::server::BuildConsoleMsg(
							UserName + " was accepted as a member of the Royal Council of Banderbill.",
							FontTypeNames_FONTTYPE_CONSEJO));
			if (UserTienePrivilegio(tUser, PlayerType_ChaosCouncil)) {
				UserQuitarPrivilegios(tUser, PlayerType_ChaosCouncil);
			}
			if (!UserTienePrivilegio(tUser, PlayerType_RoyalCouncil)) {
				UserAsignarPrivilegios(tUser, PlayerType_RoyalCouncil);
			}

			WarpUserChar(tUser, UserList[tUser].Pos.Map, UserList[tUser].Pos.X, UserList[tUser].Pos.Y, false);
		}
	}
}

/* '' */
/* ' Handles the "ChaosCouncilMember" message. */
/* ' */


void HoAClientPacketHandler::handleAcceptChaosCouncilMember(AcceptChaosCouncilMember* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		tUser = NameIndex(UserName);
		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User offline", FontTypeNames_FONTTYPE_INFO);
		} else {
			SendData(SendTarget_ToAll, 0,
					hoa::protocol::server::BuildConsoleMsg(UserName + " was accepted as a member of the Council of Shadows.",
							FontTypeNames_FONTTYPE_CONSEJO));

			if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoyalCouncil)) {
				UserQuitarPrivilegios(tUser, PlayerType_RoyalCouncil);
			}
			if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_ChaosCouncil)) {
				UserAsignarPrivilegios(tUser, PlayerType_ChaosCouncil);
			}

			WarpUserChar(tUser, UserList[tUser].Pos.Map, UserList[tUser].Pos.X, UserList[tUser].Pos.Y, false);
		}
	}




}

/* '' */
/* ' Handles the "ItemsInTheFloor" message. */
/* ' */


void HoAClientPacketHandler::handleItemsInTheFloor(ItemsInTheFloor* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	int tObj;
	std::string lista;
	int X;
	int Y;

	for (X = (5); X <= (95); X++) {
		for (Y = (5); Y <= (95); Y++) {
			tObj = MapData[UserList[UserIndex].Pos.Map][X][Y].ObjInfo.ObjIndex;
			if (tObj > 0) {
				if (ObjData[tObj].OBJType != eOBJType_otArboles) {
					WriteConsoleMsg(UserIndex, vb6::string_format("(%d, %d)", X, Y) + ObjData[tObj].Name,
							FontTypeNames_FONTTYPE_INFO);
				}
			}
		}
	}
}

/* '' */
/* ' Handles the "MakeDumb" message. */
/* ' */


void HoAClientPacketHandler::handleMakeDumb(MakeDumb* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;


	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| (UserTienePrivilegio(UserIndex, PlayerType_SemiDios)
					&& UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {

		tUser = NameIndex(UserName);
		/* 'para deteccion de aoice */
		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User offline.", FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteDumb(tUser);
		}
	}




}

/* '' */
/* ' Handles the "MakeDumbNoMore" message. */
/* ' */


void HoAClientPacketHandler::handleMakeDumbNoMore(MakeDumbNoMore* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| (UserTienePrivilegio(UserIndex, PlayerType_SemiDios)
					&& UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {
		tUser = NameIndex(UserName);
		/* 'para deteccion de aoice */
		if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "User offline.", FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteDumbNoMore(tUser);
			FlushBuffer(tUser);
		}
	}




}

/* '' */
/* ' Handles the "DumpIPTables" message. */
/* ' */


void HoAClientPacketHandler::handleDumpIPTables(DumpIPTables* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	IpSecurityDumpTables();
}

/* '' */
/* ' Handles the "CouncilKick" message. */
/* ' */


void HoAClientPacketHandler::handleCouncilKick(CouncilKick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			&& (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {

		tUser = NameIndex(UserName);
		if (tUser <= 0) {
			if (FileExist(GetCharPath(UserName))) {
				WriteConsoleMsg(UserIndex, "User offline, removing from councils.",
						FontTypeNames_FONTTYPE_INFO);
				WriteVar(GetCharPath(UserName), "CONSEJO", "PERTENECE", 0);
				WriteVar(GetCharPath(UserName), "CONSEJO", "PERTENECECAOS", 0);
			} else {
				WriteConsoleMsg(UserIndex, "Not found on charfile " + GetCharPath(UserName),
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserTieneAlgunPrivilegios(tUser, PlayerType_RoyalCouncil)) {
				WriteConsoleMsg(tUser, "You've been expeled from the Royal Council of Banderbill.",
						FontTypeNames_FONTTYPE_TALK);
				UserQuitarPrivilegios(tUser, PlayerType_RoyalCouncil);

				WarpUserChar(tUser, UserList[tUser].Pos.Map, UserList[tUser].Pos.X, UserList[tUser].Pos.Y,
						false);
				SendData(SendTarget_ToAll, 0,
						hoa::protocol::server::BuildConsoleMsg(UserName + " was expeled from the Royal Council of Banderbill.",
								FontTypeNames_FONTTYPE_CONSEJO));
			}

			if (UserTieneAlgunPrivilegios(tUser, PlayerType_ChaosCouncil)) {
				WriteConsoleMsg(tUser, "You've been expeled from the Council of Shadows.",
						FontTypeNames_FONTTYPE_TALK);
				UserQuitarPrivilegios(tUser, PlayerType_ChaosCouncil);

				WarpUserChar(tUser, UserList[tUser].Pos.Map, UserList[tUser].Pos.X, UserList[tUser].Pos.Y,
						false);
				SendData(SendTarget_ToAll, 0,
						hoa::protocol::server::BuildConsoleMsg(UserName + " was expeled from the Council of Shadows.",
								FontTypeNames_FONTTYPE_CONSEJO));
			}
		}
	}




}

/* '' */
/* ' Handles the "SetTrigger" message. */
/* ' */


void HoAClientPacketHandler::handleSetTrigger(SetTrigger* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	int tTrigger;
	std::string tLog;

	tTrigger = p->Trigger;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	if (tTrigger >= 0) {
		MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].trigger =
				static_cast<eTrigger>(tTrigger);
		tLog = vb6::string_format("Trigger %d in map %d %d %d",
				tTrigger,
				UserList[UserIndex].Pos.Map,
				UserList[UserIndex].Pos.X,
				UserList[UserIndex].Pos.Y);
		LogGM(UserList[UserIndex].Name, tLog);
		WriteConsoleMsg(UserIndex, tLog, FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "AskTrigger" message. */
/* ' */


void HoAClientPacketHandler::handleAskTrigger(AskTrigger* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 04/13/07 */
	/* ' */
	/* '*************************************************** */
	int tTrigger;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	tTrigger =
			MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].trigger;

	LogGM(UserList[UserIndex].Name,
			vb6::string_format("Looking trigger %d in map %d %d %d",
							tTrigger,
							UserList[UserIndex].Pos.Map,
							UserList[UserIndex].Pos.X,
							UserList[UserIndex].Pos.Y));

	WriteConsoleMsg(UserIndex,
			vb6::string_format("Looking %d in map %d %d %d",
							tTrigger,
							UserList[UserIndex].Pos.Map,
							UserList[UserIndex].Pos.X,
							UserList[UserIndex].Pos.Y),
			FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "BannedIPList" message. */
/* ' */


void HoAClientPacketHandler::handleBannedIPList(BannedIPList* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	std::string lista;

	LogGM(UserList[UserIndex].Name, "/BANIPLIST");

	for (auto & e : BanIps) {
		lista += e + ", ";
	}

	if (vb6::LenB(lista) != 0) {
		lista = vb6::Left(lista, vb6::Len(lista) - 2);
	}

	WriteConsoleMsg(UserIndex, lista, FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "BannedIPReload" message. */
/* ' */


void HoAClientPacketHandler::handleBannedIPReload(BannedIPReload* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	BanIpGuardar();
	BanIpCargar();
}

/* '' */
/* ' Handles the "GuildBan" message. */
/* ' */


void HoAClientPacketHandler::handleGuildBan(GuildBan* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string GuildName;
	int cantMembers;
	int LoopC;
	std::string member;
	int Count;
	int tIndex;
	std::string tFile;

	GuildName = p->GuildName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			&& (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {

		tFile = GetGuildsPath(GuildName, EGUILDPATH::Members);

		if (!FileExist(tFile)) {
			WriteConsoleMsg(UserIndex, "Clan does not exist: " + GuildName, FontTypeNames_FONTTYPE_INFO);
		} else {
			SendData(SendTarget_ToAll, 0,
					hoa::protocol::server::BuildConsoleMsg(
							UserList[UserIndex].Name + " banned clan " + vb6::UCase(GuildName),
							FontTypeNames_FONTTYPE_FIGHT));

			/* 'baneamos a los miembros */
			LogGM(UserList[UserIndex].Name, "BANCLAN to " + vb6::UCase(GuildName));

			cantMembers = vb6::val(GetVar(tFile, "INIT", "NroMembers"));
			cantMembers = vb6::Constrain(cantMembers, 0, MAXCLANMEMBERS);

			for (LoopC = (1); LoopC <= (cantMembers); LoopC++) {
				member = GetVar(tFile, "Members", "Member" + vb6::CStr(LoopC));
				/* 'member es la victima */
				Ban(member, "Server Administration", "Clan Banned");

				SendData(SendTarget_ToAll, 0,
						hoa::protocol::server::BuildConsoleMsg(
								"   " + member + "<" + GuildName + "> has been expeled from the server.",
								FontTypeNames_FONTTYPE_FIGHT));

				tIndex = NameIndex(member);
				if (tIndex > 0) {
					/* 'esta online */
					UserList[tIndex].flags.Ban = 1;
					CloseSocket(tIndex);
				}

				/* 'ponemos el flag de ban a 1 */
				WriteVar(GetCharPath(member), "FLAGS", "Ban", "1");
				/* 'ponemos la pena */
				Count = vb6::val(GetVar(GetCharPath(member), "PENAS", "Cant"));
				WriteVar(GetCharPath(member), "PENAS", "Cant", Count + 1);
				WriteVar(GetCharPath(member), "PENAS", "P" + vb6::CStr(Count + 1),
						vb6::LCase(UserList[UserIndex].Name) + ": BAN AL CLAN: " + GuildName + " "
								+ vb6::dateToString(vb6::Now()));
			}
		}
	}
}

/* '' */
/* ' Handles the "BanIP" message. */
/* ' */


void HoAClientPacketHandler::handleBanIP(BanIP* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 07/02/09 */
	/* 'Agregado un CopyBuffer porque se producia un bucle */
	/* 'inifito al intentar banear una ip ya baneada. (NicoNZ) */
	/* '07/02/09 Pato - Ahora no es posible saber si un gm está o no online. */
	/* '*************************************************** */

	std::string bannedIP;
	int tUser = 0;
	std::string Reason;
	int i;

	bannedIP = p->IP;
	Reason = p->Reason;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (vb6::LenB(bannedIP) > 0) {
			LogGM(UserList[UserIndex].Name, "/BanIP " + bannedIP + " por " + Reason);

			if (BanIpBuscar(bannedIP) > 0) {
				WriteConsoleMsg(UserIndex, "IP " + bannedIP + " is already in the banned list.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				BanIpAgrega(bannedIP);
				SendData(SendTarget_ToAdmins, 0,
						hoa::protocol::server::BuildConsoleMsg(
								UserList[UserIndex].Name + " banned IP " + bannedIP + " because of " + Reason,
								FontTypeNames_FONTTYPE_FIGHT));

				/* 'Find every player with that ip and ban him! */
				for (i = (1); i <= (LastUser); i++) {
					if (UserIndexSocketValido(i)) {
						if (UserList[i].ip == bannedIP) {
							BanCharacter(UserIndex, UserList[i].Name, "IP because of " + Reason);
						}
					}
				}
			}
		} else if (tUser <= 0) {
			WriteConsoleMsg(UserIndex, "PC is offline.", FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handles the "UnbanIP" message. */
/* ' */


void HoAClientPacketHandler::handleUnbanIP(UnbanIP* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string bannedIP = p->IP;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	if (BanIpQuita(bannedIP)) {
		WriteConsoleMsg(UserIndex, "IP " "" + bannedIP + "" " has been removed from the banned list.",
				FontTypeNames_FONTTYPE_INFO);
	} else {
		WriteConsoleMsg(UserIndex, "IP " "" + bannedIP + "" " WAS NOT found in the banned list.",
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handles the "CreateItem" message. */
/* ' */


void HoAClientPacketHandler::handleCreateItem(CreateItem* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	int tObj;
	std::string tStr;
	tObj = p->Item;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	int mapa;
	int X;
	int Y;

	mapa = UserList[UserIndex].Pos.Map;
	X = UserList[UserIndex].Pos.X;
	Y = UserList[UserIndex].Pos.Y;

	LogGM(UserList[UserIndex].Name, vb6::string_format("/CI: %d in map %d (%d, %d)", tObj, mapa, X, Y));

	if (MapData[mapa][X][Y - 1].ObjInfo.ObjIndex > 0) {
		return;
	}

	if (MapData[mapa][X][Y - 1].TileExit.Map > 0) {
		return;
	}

	if (tObj < 1 || tObj > NumObjDatas) {
		return;
	}

	/* 'Is the object not null? */
	if (vb6::LenB(ObjData[tObj].Name) == 0) {
		return;
	}

	struct Obj Objeto;
	WriteConsoleMsg(UserIndex,
			"¡¡ATENCIÓN: FUERON CREADOS ***100*** ÍTEMS, TIRE Y /DEST LOS QUE NO NECESITE!!",
			FontTypeNames_FONTTYPE_GUILD);

	Objeto.Amount = 100;
	Objeto.ObjIndex = tObj;
	MakeObj(Objeto, mapa, X, Y - 1);

	if (ObjData[tObj].Log == 1) {
		LogDesarrollo(
				UserList[UserIndex].Name
						+ vb6::string_format(" /CI: [%d] %s in map %d (%d, %d)",
								tObj, ObjData[tObj].Name.c_str(), mapa, X, Y));
	}
}

/* '' */
/* ' Handles the "DestroyItems" message. */
/* ' */


void HoAClientPacketHandler::handleDestroyItems(DestroyItems* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	int mapa;
	int X;
	int Y;

	mapa = UserList[UserIndex].Pos.Map;
	X = UserList[UserIndex].Pos.X;
	Y = UserList[UserIndex].Pos.Y;

	int ObjIndex;
	ObjIndex = MapData[mapa][X][Y].ObjInfo.ObjIndex;

	if (ObjIndex == 0) {
		return;
	}

	LogGM(UserList[UserIndex].Name,
			vb6::string_format("/DEST %d in map %d (%d, %d). Amount: %d", ObjIndex, mapa, X, Y,
					MapData[mapa][X][Y].ObjInfo.Amount));

	if (ObjData[ObjIndex].OBJType == eOBJType_otTeleport && MapData[mapa][X][Y].TileExit.Map > 0) {

		WriteConsoleMsg(UserIndex, "You can't destroy teleports like that. Use /DT.",
				FontTypeNames_FONTTYPE_INFO);
		return;
	}

	EraseObj(10000, mapa, X, Y);
}

/* '' */
/* ' Handles the "ChaosLegionKick" message. */
/* ' */


void HoAClientPacketHandler::handleChaosLegionKick(ChaosLegionKick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string Reason;
	int tUser = 0;
	int cantPenas;

	UserName = p->UserName;
	Reason = p->Reason;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| UserList[UserIndex].flags.PrivEspecial) {

		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		tUser = NameIndex(UserName);

		LogGM(UserList[UserIndex].Name, "KICKED FROM DARK LEGION: " + UserName);

		if (tUser > 0) {
			ExpulsarFaccionCaos(tUser, true);
			UserList[tUser].Faccion.Reenlistadas = 200;
			WriteConsoleMsg(UserIndex,
					UserName + " expeled from the dark legion, and prohibited re-entry.",
					FontTypeNames_FONTTYPE_INFO);
			WriteConsoleMsg(tUser,
					UserList[UserIndex].Name
							+ " has epeled you from the dark legion forever.",
					FontTypeNames_FONTTYPE_FIGHT);
			FlushBuffer(tUser);

			cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
			WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
			WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
					vb6::LCase(UserList[UserIndex].Name) + ": EXPELED from the dark legion because: "
							+ vb6::LCase(Reason) + " " + vb6::dateToString(vb6::Now()));
		} else {
			if (FileExist(GetCharPath(UserName))) {
				WriteVar(GetCharPath(UserName), "FACCIONES", "EjercitoCaos", 0);
				WriteVar(GetCharPath(UserName), "FACCIONES", "Reenlistadas", 200);
				WriteVar(GetCharPath(UserName), "FACCIONES", "Extra",
						"Expulsado por " + UserList[UserIndex].Name);

				cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
				WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
				WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
						vb6::LCase(UserList[UserIndex].Name) + ": EXPULSADO de la Legión Oscura por: "
								+ vb6::LCase(Reason) + " " + vb6::dateToString(vb6::Now()));

				WriteConsoleMsg(UserIndex,
						UserName + " expeled from the dark legion, and prohibited re-entry.",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, UserName + ".chr not found.", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "RoyalArmyKick" message. */
/* ' */


void HoAClientPacketHandler::handleRoyalArmyKick(RoyalArmyKick* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string Reason;
	int tUser = 0;
	int cantPenas;

	UserName = p->UserName;
	Reason = p->Reason;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| UserList[UserIndex].flags.PrivEspecial) {

		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		tUser = NameIndex(UserName);

		LogGM(UserList[UserIndex].Name, "KICKED FROM THE ROYAL ARMY: " + UserName);

		if (tUser > 0) {
			ExpulsarFaccionReal(tUser, true);
			UserList[tUser].Faccion.Reenlistadas = 200;
			WriteConsoleMsg(UserIndex,
					UserName + " expeled from the royal army, and prohibited re-entry.",
					FontTypeNames_FONTTYPE_INFO);
			WriteConsoleMsg(tUser,
					UserList[UserIndex].Name + " has expeled you from the royal army forever.",
					FontTypeNames_FONTTYPE_FIGHT);
			FlushBuffer(tUser);

			cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
			WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
			WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
					vb6::LCase(UserList[UserIndex].Name) + ": EXPELED from the ROYAL ARMY because: "
							+ vb6::LCase(Reason) + " " + vb6::dateToString(vb6::Now()));
		} else {
			if (FileExist(GetCharPath(UserName))) {
				WriteVar(GetCharPath(UserName), "FACCIONES", "EjercitoReal", 0);
				WriteVar(GetCharPath(UserName), "FACCIONES", "Reenlistadas", 200);
				WriteVar(GetCharPath(UserName), "FACCIONES", "Extra",
						"Expeled by " + UserList[UserIndex].Name);
				WriteConsoleMsg(UserIndex,
						UserName + " expeled from the royal army, and prohibited re-entry.",
						FontTypeNames_FONTTYPE_INFO);

				cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
				WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
				WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
						vb6::LCase(UserList[UserIndex].Name) + ": EXPELED from the ROYAL ARMY because: "
								+ vb6::LCase(Reason) + " " + vb6::dateToString(vb6::Now()));
			} else {
				WriteConsoleMsg(UserIndex, UserName + ".chr not found.", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "ForceMIDIAll" message. */
/* ' */


void HoAClientPacketHandler::handleForceMIDIAll(ForceMIDIAll* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	int midiID;
	midiID = p->MidiID;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	SendData(SendTarget_ToAll, 0,
			hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + " broadcasting music: " + vb6::CStr(midiID),
					FontTypeNames_FONTTYPE_SERVER));

	SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildPlayMidi(midiID, 0));
}

/* '' */
/* ' Handles the "ForceWAVEAll" message. */
/* ' */


void HoAClientPacketHandler::handleForceWAVEAll(ForceWAVEAll* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	int waveID;
	waveID = p->WaveID;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildPlayWave(waveID, NO_3D_SOUND, NO_3D_SOUND));
}

/* '' */
/* ' Handles the "RemovePunishment" message. */
/* ' */


void HoAClientPacketHandler::handleRemovePunishment(RemovePunishment* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 1/05/07 */
	/* 'Pablo (ToxicWaste): 1/05/07, You can now edit the punishment. */
	/* '*************************************************** */

	std::string UserName;
	int punishment;
	std::string NewText;

	UserName = p->UserName;
	punishment = p->Punishment;
	NewText = p->NewText;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			&& (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {

		if (vb6::LenB(UserName) == 0) {
			WriteConsoleMsg(UserIndex, "Use /borrarpena Nick@NumeroDePena@NuevaPena",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			if ((vb6::InStrB(UserName, "/") != 0)) {
				UserName = vb6::Replace(UserName, "/", "");
			}
			if ((vb6::InStrB(UserName, "/") != 0)) {
				UserName = vb6::Replace(UserName, "/", "");
			}

			if (FileExist(GetCharPath(UserName), 0)) {
				LogGM(UserList[UserIndex].Name,
						" borro la pena: " + vb6::CStr(punishment) + "-"
								+ GetVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(punishment)) + " de "
								+ UserName + " y la cambió por: " + NewText);

				WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(punishment),
						vb6::LCase(UserList[UserIndex].Name) + ": <" + NewText + "> "
								+ vb6::dateToString(vb6::Now()));

				WriteConsoleMsg(UserIndex, "Pena modificada.", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "TileBlockedToggle" message. */
/* ' */


void HoAClientPacketHandler::handleTileBlockedToggle(TileBlockedToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, "/BLOQ");

	if (MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].Blocked
			== 0) {
		MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].Blocked =
				1;
	} else {
		MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].Blocked =
				0;
	}

	Bloquear(true, UserList[UserIndex].Pos.Map, UserList[UserIndex].Pos.X, UserList[UserIndex].Pos.Y,
			MapData[UserList[UserIndex].Pos.Map][UserList[UserIndex].Pos.X][UserList[UserIndex].Pos.Y].Blocked);
}

/* '' */
/* ' Handles the "KillNPCNoRespawn" message. */
/* ' */


void HoAClientPacketHandler::handleKillNPCNoRespawn(KillNPCNoRespawn* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	if (UserList[UserIndex].flags.TargetNPC == 0) {
		return;
	}

	QuitarNPC(UserList[UserIndex].flags.TargetNPC);
	LogGM(UserList[UserIndex].Name, "/MATA " + Npclist[UserList[UserIndex].flags.TargetNPC].Name);
}

/* '' */
/* ' Handles the "KillAllNearbyNPCs" message. */
/* ' */


void HoAClientPacketHandler::handleKillAllNearbyNPCs(KillAllNearbyNPCs* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	int X;
	int Y;

	for (Y = (UserList[UserIndex].Pos.Y - MinYBorder + 1); Y <= (UserList[UserIndex].Pos.Y + MinYBorder - 1);
			Y++) {
		for (X = (UserList[UserIndex].Pos.X - MinXBorder + 1);
				X <= (UserList[UserIndex].Pos.X + MinXBorder - 1); X++) {
			if (X > 0 && Y > 0 && X < 101 && Y < 101) {
				if (MapData[UserList[UserIndex].Pos.Map][X][Y].NpcIndex > 0) {
					QuitarNPC(MapData[UserList[UserIndex].Pos.Map][X][Y].NpcIndex);
				}
			}
		}
	}
	LogGM(UserList[UserIndex].Name, "/MASSKILL");
}

/* '' */
/* ' Handles the "LastIP" message. */
/* ' */


void HoAClientPacketHandler::handleLastIP(LastIP* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Nicolas Matias Gonzalez (NIGO) */
	/* 'Last Modification: 12/30/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string lista;
	int LoopC;
	unsigned int priv;
	bool validCheck = false;

	priv = PlayerType_Admin | PlayerType_Dios | PlayerType_SemiDios | PlayerType_Consejero;
	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)
			&& (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster))) {
		/* 'Handle special chars */
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		if ((vb6::InStrB(UserName, "/") != 0)) {
			UserName = vb6::Replace(UserName, "/", "");
		}
		if ((vb6::InStrB(UserName, "+") != 0)) {
			UserName = vb6::Replace(UserName, "+", " ");
		}

		int TargetIndex = NameIndex(UserName);

		/* 'Only Gods and Admins can see the ips of adminsitrative characters. All others can be seen by every adminsitrative char. */
		if (TargetIndex > 0) {
			validCheck = !UserTieneAlgunPrivilegios(TargetIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios, PlayerType_Consejero)
					|| (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios));
		} else {
			validCheck = (UserDarPrivilegioLevel(UserName) && priv) == 0
					|| (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios));
		}

		if (validCheck) {
			LogGM(UserList[UserIndex].Name, "/LASTIP " + UserName);

			if (FileExist(GetCharPath(UserName), 0)) {
				lista = "The last IPs with which " + UserName + " connected to the server are:";
				for (LoopC = (1); LoopC <= (5); LoopC++) {
					lista = lista + vbCrLf + vb6::CStr(LoopC) + " - "
							+ GetVar(GetCharPath(UserName), "INIT", "LastIP" + vb6::CStr(LoopC));
				}
				WriteConsoleMsg(UserIndex, lista, FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, "Charfile " "" + UserName + "" " not found.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			WriteConsoleMsg(UserIndex, UserName + " belongs to a greater hierarchy than you.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handles the "ChatColor" message. */
/* ' */


void HoAClientPacketHandler::handleChatColor(ChatColor* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Change the user`s chat color */
	/* '*************************************************** */

	int color;

	color = vb6::RGBtoInt(p->R, p->G, p->B);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_RoleMaster)) {
		UserList[UserIndex].flags.ChatColor = color;
	}
}

/* '' */
/* ' Handles the "Ignored" message. */
/* ' */


void HoAClientPacketHandler::handleIgnored(Ignored* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Ignore the user */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios, PlayerType_Consejero)) {
		UserList[UserIndex].flags.AdminPerseguible = !UserList[UserIndex].flags.AdminPerseguible;
	}
}

/* '' */
/* ' Handles the "CheckSlot" message. */
/* ' */


void HoAClientPacketHandler::handleCheckSlot(CheckSlot* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 07/06/2010 */
	/* 'Check one Users Slot in Particular from Inventory */
	/* '07/06/2010: ZaMa - Ahora no se puede usar para saber si hay dioses/admins online. */
	/* '*************************************************** */

	/* 'Reads the UserName and Slot Packets */
	std::string UserName;
	int Slot;
	int tIndex;

	bool UserIsAdmin = false;
	bool OtherUserIsAdmin = false;

	/* 'Que UserName? */
	UserName = p->UserName;
	/* 'Que Slot? */
	Slot = p->Slot;

	UserIsAdmin = UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios);

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_SemiDios) || UserIsAdmin) {

		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name + " checked slot " + vb6::CStr(Slot) + " of " + UserName);

		/* 'Que user index? */
		tIndex = NameIndex(UserName);
		OtherUserIsAdmin = EsDios(UserName) || EsAdmin(UserName);

		if (tIndex > 0) {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				if (Slot > 0 && Slot <= UserList[tIndex].CurrentInventorySlots) {
					if (UserList[tIndex].Invent.Object[Slot].ObjIndex > 0) {
						WriteConsoleMsg(UserIndex,
								" Object " + vb6::CStr(Slot) + ") "
										+ ObjData[UserList[tIndex].Invent.Object[Slot].ObjIndex].Name
										+ " Amount:" + vb6::CStr(UserList[tIndex].Invent.Object[Slot].Amount),
								FontTypeNames_FONTTYPE_INFO);
					} else {
						WriteConsoleMsg(UserIndex, "There's no object in the selected slot.",
								FontTypeNames_FONTTYPE_INFO);
					}
				} else {
					WriteConsoleMsg(UserIndex, "Invalid slot.", FontTypeNames_FONTTYPE_TALK);
				}
			} else {
				WriteConsoleMsg(UserIndex, "You can't see slots of a god or admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		} else {
			if (UserIsAdmin || !OtherUserIsAdmin) {
				WriteConsoleMsg(UserIndex, "User offline.", FontTypeNames_FONTTYPE_TALK);
			} else {
				WriteConsoleMsg(UserIndex, "You can't see slots of a god or admin.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}




}

/* '' */
/* ' Handles the "ResetAutoUpdate" message. */
/* ' */


void HoAClientPacketHandler::handleResetAutoUpdate(ResetAutoUpdate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Reset the AutoUpdate */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

#if 0
	if (vb6::UCase(UserList[UserIndex].Name) != "MARAXUS") {
		return;
	}
#endif

	WriteConsoleMsg(UserIndex, "No,no no... command disabled.", FontTypeNames_FONTTYPE_INFO);

	// WriteConsoleMsg(UserIndex, "TID: " + vb6::CStr(ReiniciarAutoUpdate()), FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "Restart" message. */
/* ' */


void HoAClientPacketHandler::handleRestart(hoa::protocol::clientgm::Restart* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Restart the game */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

//	if (vb6::UCase(UserList[UserIndex].Name) != "MARAXUS") {
//		return;
//	}

	/* 'time and Time BUG! */
	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " resetted the world.");

#if 0
	ReiniciarServidor(true);
#endif

	WriteConsoleMsg(UserIndex, "No,no no... command disabled.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "ReloadObjects" message. */
/* ' */


void HoAClientPacketHandler::handleReloadObjects(ReloadObjects* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Reload the objects */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has reloaded the objects.");

	LoadOBJData();

	WriteConsoleMsg(UserIndex, "OBJ.dat has been reloaded.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handles the "ReloadSpells" message. */
/* ' */


void HoAClientPacketHandler::handleReloadSpells(ReloadSpells* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Reload the spells */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has reloaded all spells.");

	CargarHechizos();

	WriteConsoleMsg(UserIndex, "Hechizos.dat has been reloaded.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "ReloadServerIni" message. */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleReloadServerIni(ReloadServerIni* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Reload the Server`s INI */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has reloaded INITs: server.ini and MOTD");

	LoadSini();
	
	LoadMotd();

	WriteConsoleMsg(UserIndex, "Server.ini and MOTD updated correctly.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "ReloadNPCs" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleReloadNPCs(ReloadNPCs* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Reload the Server`s NPC */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has reloaded all NPCs.");

	CargaNpcsDat();

	WriteConsoleMsg(UserIndex, "Npcs.dat reloaded.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "KickAllChars" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleKickAllChars(KickAllChars* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Kick all the chars that are online */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has kicked all PCs.");

	EcharPjsNoPrivilegiados();
}

/* '' */
/* ' Handle the "Night" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleNight(Night* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

#if 0
	if (vb6::UCase(UserList[UserIndex].Name) != "MARAXUS") {
		return;
	}
#endif

	DeNoche = !DeNoche;

	int i;

	for (i = (1); i <= (NumUsers); i++) {
		if (UserList[i].flags.UserLogged && UserIndexSocketValido(i)) {
			EnviarNoche(i);
		}
	}
}

/* '' */
/* ' Handle the "ShowServerForm" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleShowServerForm(ShowServerForm* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Show the server form */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name,
			UserList[UserIndex].Name + " has requested to show the server's form.");
}

/* '' */
/* ' Handle the "CleanSOS" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleCleanSOS(CleanSOS* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Clean the SOS */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has deleted all SOS.");

	Ayuda.clear();
}

/* '' */
/* ' Handle the "SaveChars" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSaveChars(SaveChars* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/23/06 */
	/* 'Save the characters */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " has saved all chars.");

	ActualizaExperiencias();
	GuardarUsuarios();
}

/* '' */
/* ' Handle the "ChangeMapInfoBackup" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoBackup(ChangeMapInfoBackup* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Change the backup`s info of the map */
	/* '*************************************************** */

	bool doTheBackUp;

	doTheBackUp = p->Backup;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name,
			UserList[UserIndex].Name + " has changed the information on the backup.");

	/* 'Change the boolean to byte in a fast way */
	if (doTheBackUp) {
		MapInfo[UserList[UserIndex].Pos.Map].BackUp = 1;
	} else {
		MapInfo[UserList[UserIndex].Pos.Map].BackUp = 0;
	}

	/* 'Change the boolean to string in a fast way */
	WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
			"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "backup", vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].BackUp));

	WriteConsoleMsg(UserIndex,
			"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " Backup: " + vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].BackUp),
			FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "ChangeMapInfoPK" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoPK(ChangeMapInfoPK* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Change the pk`s info of the  map */
	/* '*************************************************** */

	bool isMapPk;

	isMapPk = p->Pk;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name,
			UserList[UserIndex].Name + " has changed the map information.");

	MapInfo[UserList[UserIndex].Pos.Map].Pk = isMapPk;

	/* 'Change the boolean to string in a fast way */
	WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
			"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "Pk", vb6::IIf(isMapPk, "1", "0"));

	WriteConsoleMsg(UserIndex,
			"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " PK: " + vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].Pk),
			FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "ChangeMapInfoRestricted" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoRestricted(ChangeMapInfoRestricted* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'Restringido -> Options: "NEWBIE", "NO", "ARMADA", "CAOS", "FACCION". */
	/* '*************************************************** */

	std::string tStr;

	tStr = p->RestrictedTo;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (tStr == "NEWBIE" || tStr == "NO" || tStr == "ARMADA" || tStr == "CAOS" || tStr == "FACCION") {
			LogGM(UserList[UserIndex].Name,
					UserList[UserIndex].Name
							+ " ha cambiado la información sobre si es restringido el mapa.");

			MapInfo[UserList[UserIndex].Pos.Map].Restringir = RestrictStringToByte(tStr);

			WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
					"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "Restringir", tStr);
			WriteConsoleMsg(UserIndex,
					"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " Restringido: "
							+ RestrictByteToString(MapInfo[UserList[UserIndex].Pos.Map].Restringir),
					FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteConsoleMsg(UserIndex,
					"Opciones para restringir: 'NEWBIE', 'NO', 'ARMADA', 'CAOS', 'FACCION'",
					FontTypeNames_FONTTYPE_INFO);
		}
	}




}

/* '' */
/* ' Handle the "ChangeMapInfoNoMagic" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoNoMagic(ChangeMapInfoNoMagic* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'MagiaSinEfecto -> Options: "1" , "0". */
	/* '*************************************************** */

	bool nomagic;

	nomagic = p->NoMagic;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido usar la magia el mapa.");
		MapInfo[UserList[UserIndex].Pos.Map].MagiaSinEfecto = nomagic;
		WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
				"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "MagiaSinEfecto", nomagic);
		WriteConsoleMsg(UserIndex,
				"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " MagiaSinEfecto: "
						+ vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].MagiaSinEfecto), FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handle the "ChangeMapInfoNoInvi" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoNoInvi(ChangeMapInfoNoInvi* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'InviSinEfecto -> Options: "1", "0" */
	/* '*************************************************** */

	bool noinvi;

	noinvi = p->NoInvi;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido usar la invisibilidad en el mapa.");
		MapInfo[UserList[UserIndex].Pos.Map].InviSinEfecto = noinvi;
		WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
				"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "InviSinEfecto", noinvi);
		WriteConsoleMsg(UserIndex,
				"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " InviSinEfecto: "
						+ vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].InviSinEfecto), FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handle the "ChangeMapInfoNoResu" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoNoResu(ChangeMapInfoNoResu* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'ResuSinEfecto -> Options: "1", "0" */
	/* '*************************************************** */

	bool noresu;

	noresu = p->NoResu;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido usar el resucitar en el mapa.");
		MapInfo[UserList[UserIndex].Pos.Map].ResuSinEfecto = noresu;
		WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
				"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "ResuSinEfecto", noresu);
		WriteConsoleMsg(UserIndex,
				"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " ResuSinEfecto: "
						+ vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].ResuSinEfecto), FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handle the "ChangeMapInfoLand" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoLand(ChangeMapInfoLand* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'Terreno -> Opciones: "BOSQUE", "NIEVE", "DESIERTO", "CIUDAD", "CAMPO", "DUNGEON". */
	/* '*************************************************** */

	std::string tStr;
	tStr = p->Data;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (tStr == "BOSQUE" || tStr == "NIEVE" || tStr == "DESIERTO" || tStr == "CIUDAD" || tStr == "CAMPO"
				|| tStr == "DUNGEON") {
			LogGM(UserList[UserIndex].Name,
					UserList[UserIndex].Name + " ha cambiado la información del terreno del mapa.");

			MapInfo[UserList[UserIndex].Pos.Map].Terreno = TerrainStringToByte(tStr);

			WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
					"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "Terreno", tStr);
			WriteConsoleMsg(UserIndex,
					"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " Terreno: "
							+ vb6::CStr(TerrainByteToString(MapInfo[UserList[UserIndex].Pos.Map].Terreno)),
					FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteConsoleMsg(UserIndex,
					"Opciones para terreno: 'BOSQUE', 'NIEVE', 'DESIERTO', 'CIUDAD', 'CAMPO', 'DUNGEON'",
					FontTypeNames_FONTTYPE_INFO);
			WriteConsoleMsg(UserIndex,
					"Igualmente, el único útil es 'NIEVE' ya que al ingresarlo, la gente muere de frío en el mapa.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}




}

/* '' */
/* ' Handle the "ChangeMapInfoZone" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoZone(ChangeMapInfoZone* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Pablo (ToxicWaste) */
	/* 'Last Modification: 26/01/2007 */
	/* 'Zona -> Opciones: "BOSQUE", "NIEVE", "DESIERTO", "CIUDAD", "CAMPO", "DUNGEON". */
	/* '*************************************************** */

	std::string tStr;

	tStr = p->Data;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (tStr == "BOSQUE" || tStr == "NIEVE" || tStr == "DESIERTO" || tStr == "CIUDAD" || tStr == "CAMPO"
				|| tStr == "DUNGEON") {
			LogGM(UserList[UserIndex].Name,
					UserList[UserIndex].Name + " ha cambiado la información de la zona del mapa.");
			MapInfo[UserList[UserIndex].Pos.Map].Zona = tStr;
			WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
					"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "Zona", tStr);
			WriteConsoleMsg(UserIndex,
					"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " Zona: "
							+ vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].Zona), FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteConsoleMsg(UserIndex,
					"Opciones para terreno: 'BOSQUE', 'NIEVE', 'DESIERTO', 'CIUDAD', 'CAMPO', 'DUNGEON'",
					FontTypeNames_FONTTYPE_INFO);
			WriteConsoleMsg(UserIndex,
					"Igualmente, el único útil es 'DUNGEON' ya que al ingresarlo, NO se sentirá el efecto de la lluvia en este mapa.",
					FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handle the "ChangeMapInfoStealNp" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoStealNpc(ChangeMapInfoStealNpc* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 25/07/2010 */
	/* 'RoboNpcsPermitido -> Options: "1", "0" */
	/* '*************************************************** */

	int RoboNpc;

	RoboNpc = p->RoboNpc ? 1 : 0;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido robar npcs en el mapa.");

		MapInfo[UserList[UserIndex].Pos.Map].RoboNpcsPermitido = RoboNpc;

		WriteVar(GetMapPath(UserList[UserIndex].Pos.Map, MAPPATH::DAT),
				"Mapa" + vb6::CStr(UserList[UserIndex].Pos.Map), "RoboNpcsPermitido", RoboNpc);
		WriteConsoleMsg(UserIndex,
				"Mapa " + vb6::CStr(UserList[UserIndex].Pos.Map) + " RoboNpcsPermitido: "
						+ vb6::CStr(MapInfo[UserList[UserIndex].Pos.Map].RoboNpcsPermitido),
				FontTypeNames_FONTTYPE_INFO);
	}
}

/* '' */
/* ' Handle the "ChangeMapInfoNoOcultar" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoNoOcultar(ChangeMapInfoNoOcultar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 18/09/2010 */
	/* 'OcultarSinEfecto -> Options: "1", "0" */
	/* '*************************************************** */

	int NoOcultar;
	int mapa;

	NoOcultar = p->NoOcultar ? 1 : 0;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {

		mapa = UserList[UserIndex].Pos.Map;

		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido ocultarse en el mapa " + vb6::CStr(mapa)
						+ ".");

		MapInfo[mapa].OcultarSinEfecto = NoOcultar;

		WriteVar(GetMapPath(mapa, MAPPATH::DAT), "Mapa" + vb6::CStr(mapa), "OcultarSinEfecto",
				vb6::CStr(NoOcultar));
		WriteConsoleMsg(UserIndex, "Mapa " + vb6::CStr(mapa) + " OcultarSinEfecto: " + vb6::CStr(NoOcultar),
				FontTypeNames_FONTTYPE_INFO);
	}

}

/* '' */
/* ' Handle the "ChangeMapInfoNoInvocar" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMapInfoNoInvocar(ChangeMapInfoNoInvocar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 18/09/2010 */
	/* 'InvocarSinEfecto -> Options: "1", "0" */
	/* '*************************************************** */

	int NoInvocar;
	int mapa;

	NoInvocar = p->NoInvocar ? 1 : 0;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {

		mapa = UserList[UserIndex].Pos.Map;

		LogGM(UserList[UserIndex].Name,
				UserList[UserIndex].Name
						+ " ha cambiado la información sobre si está permitido invocar en el mapa " + vb6::CStr(mapa)
						+ ".");

		MapInfo[mapa].InvocarSinEfecto = NoInvocar;

		WriteVar(GetMapPath(mapa, MAPPATH::DAT), "Mapa" + vb6::CStr(mapa), "InvocarSinEfecto",
				NoInvocar);
		WriteConsoleMsg(UserIndex, "Mapa " + vb6::CStr(mapa) + " InvocarSinEfecto: " + vb6::CStr(NoInvocar),
				FontTypeNames_FONTTYPE_INFO);
	}

}

/* '' */
/* ' Handle the "SaveMap" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSaveMap(SaveMap* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Saves the map */
	/* '*************************************************** */



	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name,
			UserList[UserIndex].Name + " ha guardado el mapa " + vb6::CStr(UserList[UserIndex].Pos.Map));

	GrabarMapa(UserList[UserIndex].Pos.Map, true);

	WriteConsoleMsg(UserIndex, "Mapa Guardado.", FontTypeNames_FONTTYPE_INFO);
}

/* '' */
/* ' Handle the "ShowGuildMessages" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleShowGuildMessages(ShowGuildMessages* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Allows admins to read guild messages */
	/* '*************************************************** */

	std::string guild;

	guild = p->GuildName;

	if (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster) && UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		GMEscuchaClan(UserIndex, guild);
	}

}

/* '' */
/* ' Handle the "DoBackUp" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleDoBackUp(hoa::protocol::clientgm::DoBackUp* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Show guilds messages */
	/* '*************************************************** */

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, UserList[UserIndex].Name + " ha hecho un backup.");

	/* 'Sino lo confunde con la id del paquete */
	::DoBackUp();
}

/* '' */
/* ' Handle the "ToggleCentinelActivated" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleToggleCentinelActivated(ToggleCentinelActivated* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/26/06 */
	/* 'Last modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Activate or desactivate the Centinel */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	centinelaActivado = !centinelaActivado;

	ResetCentinelas();

	if (centinelaActivado) {
		SendData(SendTarget_ToAdmins, 0,
				hoa::protocol::server::BuildConsoleMsg("El centinela ha sido activado.", FontTypeNames_FONTTYPE_SERVER));
	} else {
		SendData(SendTarget_ToAdmins, 0,
				hoa::protocol::server::BuildConsoleMsg("El centinela ha sido desactivado.", FontTypeNames_FONTTYPE_SERVER));
	}
}

/* '' */
/* ' Handle the "AlterName" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleAlterName(AlterName* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/26/06 */
	/* 'Change user name */

	/* 'Reads the userName and newUser Packets */
	std::string UserName;
	std::string newName;
	int changeNameUI;
	int GuildIndex;

	UserName = p->UserName;
	newName = p->NewName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| UserList[UserIndex].flags.PrivEspecial) {
		if (vb6::LenB(UserName) == 0 || vb6::LenB(newName) == 0) {
			WriteConsoleMsg(UserIndex, "Usar: /ANAME origen@destino", FontTypeNames_FONTTYPE_INFO);
		} else {
			changeNameUI = NameIndex(UserName);

			if (changeNameUI > 0) {
				WriteConsoleMsg(UserIndex, "El Pj está online, debe salir para hacer el cambio.",
						FontTypeNames_FONTTYPE_WARNING);
			} else {
				if (!FileExist(GetCharPath(UserName))) {
					WriteConsoleMsg(UserIndex, "El pj " + UserName + " es inexistente.",
							FontTypeNames_FONTTYPE_INFO);
				} else {
					GuildIndex = vb6::val(GetVar(GetCharPath(UserName), "GUILD", "GUILDINDEX"));

					if (GuildIndex > 0) {
						WriteConsoleMsg(UserIndex,
								"El pj " + UserName
										+ " pertenece a un clan, debe salir del mismo con /salirclan para ser transferido.",
								FontTypeNames_FONTTYPE_INFO);
					} else {
						if (!FileExist(GetCharPath(UserName))) {
							vb6::FileCopy(GetCharPath(UserName),
									GetCharPath(newName));

							WriteConsoleMsg(UserIndex, "Transferencia exitosa.", FontTypeNames_FONTTYPE_INFO);

							WriteVar(GetCharPath(UserName), "FLAGS", "Ban", "1");

							int cantPenas;

							cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));

							WriteVar(GetCharPath(UserName), "PENAS", "Cant", vb6::CStr(cantPenas + 1));

							WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
									vb6::LCase(UserList[UserIndex].Name) + ": BAN POR Cambio de nick a "
											+ vb6::UCase(newName) + " " + vb6::dateToString(vb6::Now()));

							LogGM(UserList[UserIndex].Name,
									"Ha cambiado de nombre al usuario " + UserName + ". Ahora se llama "
											+ newName);
						} else {
							WriteConsoleMsg(UserIndex, "El nick solicitado ya existe.",
									FontTypeNames_FONTTYPE_INFO);
						}
					}
				}
			}
		}
	}



}

/* '' */
/* ' Handle the "AlterName" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleAlterMail(AlterMail* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/26/06 */
	/* '*************************************************** */

	std::string UserName;
	std::string newMail;

	UserName = p->UserName;
	newMail = p->NewMail;

	if (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster) &&
			UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (vb6::LenB(UserName) == 0 || vb6::LenB(newMail) == 0) {
			WriteConsoleMsg(UserIndex, "usar /AEMAIL <pj>-<nuevomail>", FontTypeNames_FONTTYPE_INFO);
		} else {
			if (!FileExist(GetCharPath(UserName))) {
				WriteConsoleMsg(UserIndex, "No existe el charfile " + UserName + ".chr",
						FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteVar(GetCharPath(UserName), "CONTACTO", "Email", newMail);
				WriteConsoleMsg(UserIndex, "Email de " + UserName + " cambiado a: " + newMail,
						FontTypeNames_FONTTYPE_INFO);
			}

			LogGM(UserList[UserIndex].Name, "Le ha cambiado el mail a " + UserName);
		}
	}
}

/* '' */
/* ' Handle the "HandleCreateNPC" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleCreateNPC(CreateNPC* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 26/09/2010 */
	/* '26/09/2010: ZaMa - Ya no se pueden crear npcs pretorianos. */
	/* '*************************************************** */

	int NpcIndex;

	NpcIndex = p->NpcIndex;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
		return;
	}

	/* FIXME: PRETORIANOS */
#if 0
	if (NpcIndex >= 900) {
		WriteConsoleMsg(UserIndex,
				"No puedes sumonear miembros del clan pretoriano de esta forma, utiliza /CrearClanPretoriano.",
				FontTypeNames_FONTTYPE_WARNING);
		return;
	}
#endif

	NpcIndex = SpawnNpc(NpcIndex, UserList[UserIndex].Pos, true, false);

	if (NpcIndex != 0) {
		LogGM(UserList[UserIndex].Name,
				"Sumoneó a " + Npclist[NpcIndex].Name + " en mapa " + vb6::CStr(UserList[UserIndex].Pos.Map));
	}
}

/* '' */
/* ' Handle the "CreateNPCWithRespawn" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleCreateNPCWithRespawn(CreateNPCWithRespawn* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 26/09/2010 */
	/* '26/09/2010: ZaMa - Ya no se pueden crear npcs pretorianos. */
	/* '*************************************************** */

	int NpcIndex;

	NpcIndex = p->NpcIndex;

	if (NpcIndex >= 900) {
		WriteConsoleMsg(UserIndex,
				"No puedes sumonear miembros del clan pretoriano de esta forma, utiliza /CrearClanPretoriano.",
				FontTypeNames_FONTTYPE_WARNING);
		return;
	}

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	NpcIndex = SpawnNpc(NpcIndex, UserList[UserIndex].Pos, true, true);

	if (NpcIndex != 0) {
		LogGM(UserList[UserIndex].Name,
				"Sumoneó con respawn " + Npclist[NpcIndex].Name + " en mapa " + vb6::CStr(UserList[UserIndex].Pos.Map));
	}
}

/* '' */
/* ' Handle the "ImperialArmour" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleImperialArmour(ImperialArmour* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/24/06 */
	/* ' */
	/* '*************************************************** */

	int index;
	int ObjIndex;

	index = p->Index;
	ObjIndex = p->ObjIndex;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	switch (index) {
	case 1:
		ArmaduraImperial1 = ObjIndex;

		break;

	case 2:
		ArmaduraImperial2 = ObjIndex;

		break;

	case 3:
		ArmaduraImperial3 = ObjIndex;

		break;

	case 4:
		TunicaMagoImperial = ObjIndex;
		break;
	}
}

/* '' */
/* ' Handle the "ChaosArmour" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChaosArmour(ChaosArmour* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/24/06 */
	/* ' */
	/* '*************************************************** */

	int index;
	int ObjIndex;

	index = p->Index;
	ObjIndex = p->ObjIndex;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	switch (index) {
	case 1:
		ArmaduraCaos1 = ObjIndex;

		break;

	case 2:
		ArmaduraCaos2 = ObjIndex;

		break;

	case 3:
		ArmaduraCaos3 = ObjIndex;

		break;

	case 4:
		TunicaMagoCaos = ObjIndex;
		break;
	}
}

/* '' */
/* ' Handle the "NavigateToggle" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleNavigateToggle(NavigateToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 01/12/07 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero)) {
		return;
	}

	if (UserList[UserIndex].flags.Navegando == 1) {
		UserList[UserIndex].flags.Navegando = 0;
	} else {
		UserList[UserIndex].flags.Navegando = 1;
	}

	/* 'Tell the client that we are navigating. */
	WriteNavigateToggle(UserIndex);
}

/* '' */
/* ' Handle the "ServerOpenToUsersToggle" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleServerOpenToUsersToggle(ServerOpenToUsersToggle* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/24/06 */
	/* ' */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios, PlayerType_RoleMaster)) {
		return;
	}

	if (ServerSoloGMs > 0) {
		WriteConsoleMsg(UserIndex, "Server online for everyone.", FontTypeNames_FONTTYPE_INFO);
		ServerSoloGMs = 0;
	} else {
		WriteConsoleMsg(UserIndex, "Server restricted to admins.", FontTypeNames_FONTTYPE_INFO);
		ServerSoloGMs = 1;
	}
}

/* '' */
/* ' Handle the "TurnOffServer" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleTurnOffServer(TurnOffServer* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/24/06 */
	/* 'Turns off the server */
	/* '*************************************************** */

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios,
			PlayerType_RoleMaster)) {
		return;
	}

	LogGM(UserList[UserIndex].Name, "/APAGAR");
	SendData(SendTarget_ToAll, 0,
			hoa::protocol::server::BuildConsoleMsg("¡¡¡" + UserList[UserIndex].Name + " VA A APAGAR EL SERVIDOR!!!",
					FontTypeNames_FONTTYPE_FIGHT));

	LogMain(vb6::string_format("%d server apagado por %s", vb6::Now(), UserList[UserIndex].Name.c_str()));
}

/* '' */
/* ' Handle the "TurnCriminal" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleTurnCriminal(TurnCriminal* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/26/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int tUser = 0;

	UserName = p->UserName;

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {

		LogGM(UserList[UserIndex].Name, "/CONDEN " + UserName);

		tUser = NameIndex(UserName);
		if (tUser > 0) {
			VolverCriminal(tUser);
		}
	}




}

/* '' */
/* ' Handle the "ResetFactions" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleResetFactions(ResetFactions* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 06/09/09 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	std::string Reason;
	int tUser = 0;
	std::string Char;
	int cantPenas;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| UserList[UserIndex].flags.PrivEspecial) {
		LogGM(UserList[UserIndex].Name, "/RAJAR " + UserName);

		tUser = NameIndex(UserName);

		if (tUser > 0) {
			ResetFacciones(tUser);

			cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
			WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
			WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
					vb6::LCase(UserList[UserIndex].Name) + ": Personaje reincorporado a la facción. "
							+ vb6::dateToString(vb6::Now()));
		} else {
			Char = GetCharPath(UserName);

			if (FileExist(Char, 0)) {
				WriteVar(Char, "FACCIONES", "EjercitoReal", 0);
				WriteVar(Char, "FACCIONES", "CiudMatados", 0);
				WriteVar(Char, "FACCIONES", "CrimMatados", 0);
				WriteVar(Char, "FACCIONES", "EjercitoCaos", 0);
				WriteVar(Char, "FACCIONES", "FechaIngreso", "No ingresó a ninguna Facción");
				WriteVar(Char, "FACCIONES", "rArCaos", 0);
				WriteVar(Char, "FACCIONES", "rArReal", 0);
				WriteVar(Char, "FACCIONES", "rExCaos", 0);
				WriteVar(Char, "FACCIONES", "rExReal", 0);
				WriteVar(Char, "FACCIONES", "recCaos", 0);
				WriteVar(Char, "FACCIONES", "recReal", 0);
				WriteVar(Char, "FACCIONES", "Reenlistadas", 0);
				WriteVar(Char, "FACCIONES", "NivelIngreso", 0);
				WriteVar(Char, "FACCIONES", "MatadosIngreso", 0);
				WriteVar(Char, "FACCIONES", "NextRecompensa", 0);

				cantPenas = vb6::val(GetVar(GetCharPath(UserName), "PENAS", "Cant"));
				WriteVar(GetCharPath(UserName), "PENAS", "Cant", cantPenas + 1);
				WriteVar(GetCharPath(UserName), "PENAS", "P" + vb6::CStr(cantPenas + 1),
						vb6::LCase(UserList[UserIndex].Name) + ": Personaje reincorporado a la facción. "
								+ vb6::dateToString(vb6::Now()));
			} else {
				WriteConsoleMsg(UserIndex, "PC " + UserName + " does not exist.",
						FontTypeNames_FONTTYPE_INFO);
			}
		}
	}
}

/* '' */
/* ' Handle the "RemoveCharFromGuild" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleRemoveCharFromGuild(RemoveCharFromGuild* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/26/06 */
	/* ' */
	/* '*************************************************** */

	std::string UserName;
	int GuildIndex;

	UserName = p->UserName;

	if (!UserTienePrivilegio(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {

		LogGM(UserList[UserIndex].Name, "/RAJARCLAN " + UserName);

		GuildIndex = m_EcharMiembroDeClan(UserIndex, UserName);

		if (GuildIndex == 0) {
			WriteConsoleMsg(UserIndex, "Does not belong to a clan or is a founder.",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			WriteConsoleMsg(UserIndex, "Expelled.", FontTypeNames_FONTTYPE_INFO);
			SendData(SendTarget_ToGuildMembers, GuildIndex,
					hoa::protocol::server::BuildConsoleMsg(
							UserName + " has been expelled from the clan by the server's admins.",
							FontTypeNames_FONTTYPE_GUILD));
		}
	}




}

/* '' */
/* ' Handle the "RequestCharMail" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleRequestCharMail(RequestCharMail* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín Sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/26/06 */
	/* 'Request user mail */
	/* '*************************************************** */

	std::string UserName;
	std::string mail;

	UserName = p->UserName;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)
			|| UserList[UserIndex].flags.PrivEspecial) {
		if (FileExist(GetCharPath(UserName))) {
			mail = GetVar(GetCharPath(UserName), "CONTACTO", "email");

			WriteConsoleMsg(UserIndex, "Last email de " + UserName + ":" + mail, FontTypeNames_FONTTYPE_INFO);
		}
	}
}

/* '' */
/* ' Handle the "SystemMessage" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSystemMessage(SystemMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/29/06 */
	/* 'Send a message to all the users */
	/* '*************************************************** */

	std::string& message = p->Message;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name, "Mensaje de sistema:" + message);

		SendData(SendTarget_ToAll, 0, hoa::protocol::server::BuildShowMessageBox(message));
	}
}

/* '' */
/* ' Handle the "SetMOTD" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSetMOTD(SetMOTD* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 03/31/07 */
	/* 'Set the MOTD */
	/* 'Modified by: Juan Martín Sotuyo Dodero (Maraxus) */
	/* '   - Fixed a bug that prevented from properly setting the new number of lines. */
	/* '   - Fixed a bug that caused the player to be kicked. */
	/* '*************************************************** */

	std::string newMOTD;
	std::vector<std::string> auxiliaryString;
	int LoopC;

	newMOTD = p->Motd;
	auxiliaryString = vb6::Split(newMOTD, vbCrLf);

	if (((!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster)
			&& UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)))
			|| UserList[UserIndex].flags.PrivEspecial) {

		LogGM(UserList[UserIndex].Name, "Ha fijado un nuevo MOTD");

		int MaxLines = vb6::UBound(auxiliaryString) + 1;

		MOTD.redim(1, MaxLines);

		WriteVar(GetDatPath(DATPATH::MOTD), "INIT", "NumLines", vb6::CStr(MaxLines));

		for (LoopC = (1); LoopC <= (MaxLines); LoopC++) {
			WriteVar(GetDatPath(DATPATH::MOTD), "Motd", "Line" + vb6::CStr(LoopC),
					auxiliaryString[LoopC - 1]);

			MOTD[LoopC].texto = auxiliaryString[LoopC - 1];
		}

		WriteConsoleMsg(UserIndex, "Se ha cambiado el MOTD con éxito.", FontTypeNames_FONTTYPE_INFO);
	}




}

/* '' */
/* ' Handle the "ChangeMOTD" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleChangeMOTD(ChangeMOTD* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Juan Martín sotuyo Dodero (Maraxus) */
	/* 'Last Modification: 12/29/06 */
	/* 'Change the MOTD */
	/* '*************************************************** */



	if ((!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin))) {
		return;
	}

	std::string auxiliaryString;
	int LoopC;

	for (LoopC = (vb6::LBound(MOTD)); LoopC <= (vb6::UBound(MOTD)); LoopC++) {
		auxiliaryString = auxiliaryString + MOTD[LoopC].texto + vbCrLf;
	}

	if (vb6::Len(auxiliaryString) >= 2) {
		if (vb6::Right(auxiliaryString, 2) == vbCrLf) {
			auxiliaryString = vb6::Left(auxiliaryString, vb6::Len(auxiliaryString) - 2);
		}
	}

	WriteShowMOTDEditionForm(UserIndex, auxiliaryString);
}

/* '' */
/* ' Handle the "Ping" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handlePing(Ping* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lucas Tavolaro Ortiz (Tavo) */
	/* 'Last Modification: 12/24/06 */
	/* 'Show guilds messages */
	/* '*************************************************** */

	WritePong(UserIndex);
}

/* '' */
/* ' Handle the "SetIniVar" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSetIniVar(SetIniVar* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Brian Chaia (BrianPr) */
	/* 'Last Modification: 01/23/10 (Marco) */
	/* 'Modify server.ini */
	/* '*************************************************** */

	std::string sLlave;
	std::string sClave;
	std::string sValor;

	/* 'Obtengo los parámetros */
	sLlave = p->Seccion;
	sClave = p->Clave;
	sValor = p->Valor;

	WriteConsoleMsg(UserIndex, "Comando deshabilitado.", FontTypeNames_FONTTYPE_INFO);
	return;
#if 0
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		std::string sTmp;

		/* 'No podemos modificar [INIT]Dioses ni [Dioses]* */
		if ((vb6::UCase(sLlave) == "INIT" && vb6::UCase(sClave) == "DIOSES")
				|| vb6::UCase(sLlave) == "DIOSES") {
			WriteConsoleMsg(UserIndex, "¡No puedes modificar esa información desde aquí!",
					FontTypeNames_FONTTYPE_INFO);
		} else {
			/* 'Obtengo el valor según llave y clave */
			sTmp = GetVar(GetIniPath("server.ini"), sLlave, sClave);

			/* 'Si obtengo un valor escribo en el server.ini */
			if (vb6::LenB(sTmp)) {
				WriteVar(GetIniPath("server.ini"), sLlave, sClave, sValor);
				LogGM(UserList[UserIndex].Name,
						"Modificó en server.ini (" + sLlave + " " + sClave + ") el valor " + sTmp + " por "
								+ sValor);
				WriteConsoleMsg(UserIndex,
						"Modificó " + sLlave + " " + sClave + " a " + sValor + ". Valor anterior " + sTmp,
						FontTypeNames_FONTTYPE_INFO);
			} else {
				WriteConsoleMsg(UserIndex, "No existe la llave y/o clave", FontTypeNames_FONTTYPE_INFO);
			}
		}
	}


#endif
}

/* '' */
/* ' Handle the "CreatePretorianClan" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleCreatePretorianClan(CreatePretorianClan* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 29/10/2010 */
	/* '*************************************************** */

	int Map = p->Map;
	int X = p->X;
	int Y = p->Y;

	/* ' User Admin? */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	/* ' Valid pos? */
	if (!InMapBounds(Map, X, Y)) {
		WriteConsoleMsg(UserIndex, "Posición inválida.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	WriteConsoleMsg(UserIndex, "Pretorianos deshabilitados.", FontTypeNames_FONTTYPE_INFO);

#if 0
	/* ' Choose pretorian clan index */
	if (Map == MAPA_PRETORIANO) {
		/* ' Default clan */
		index = 1;
	} else {
		/* ' Custom Clan */
		index = 2;
	}

	/* ' Is already active any clan? */
	if (!ClanPretoriano[index].Active) {

		if (!ClanPretoriano[index].SpawnClan(Map, X, Y, index)) {
			WriteConsoleMsg(UserIndex, "La posición no es apropiada para crear el clan", FontTypeNames_FONTTYPE_INFO);
		}

	} else {
		WriteConsoleMsg(UserIndex, "El clan pretoriano se encuentra activo en el mapa " + vb6::CStr(ClanPretoriano[index].ClanMap) + ". Utilice /EliminarPretorianos MAPA y reintente.", FontTypeNames_FONTTYPE_INFO);
	}

	LogGM(UserList[UserIndex].Name, "Utilizó el comando /CREARPRETORIANOS " + vb6::CStr(Map) + " " + vb6::CStr(X) + " " + vb6::CStr(Y));
#endif
}

/* '' */
/* ' Handle the "CreatePretorianClan" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleRemovePretorianClan(RemovePretorianClan* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 29/10/2010 */
	/* '*************************************************** */

	int Map = p->Map;

	/* ' User Admin? */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		return;
	}

	/* ' Valid map? */
	if (Map < 1 || Map > NumMaps) {
		WriteConsoleMsg(UserIndex, "Mapa inválido.", FontTypeNames_FONTTYPE_INFO);
		return;
	}

	WriteConsoleMsg(UserIndex, "Pretorianos deshabilitados.", FontTypeNames_FONTTYPE_INFO);

	/* FIXME: PRETORIANOS */

#if 0
	for (index = (1); index <= (vb6::UBound(ClanPretoriano)); index++) {

		/* ' Search for the clan to be deleted */
		if (ClanPretoriano[index].ClanMap == Map) {
			ClanPretoriano[index].DeleteClan();
			break;
		}

	}

	LogGM(UserList[UserIndex].Name, "Utilizó el comando /ELIMINARPRETORIANOS " + vb6::CStr(Map));
#endif
}

/* '' */
/* ' Handles the "SetDialog" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message */

void HoAClientPacketHandler::handleSetDialog(SetDialog* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modification: 18/11/2010 */
	/* '20/11/2010: ZaMa - Arreglo privilegios. */
	/* '*************************************************** */

	std::string& NewDialog = p->Message;

	if (UserList[UserIndex].flags.TargetNPC > 0) {
		/* ' Dsgm/Dsrm/Rm */
		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
			/* 'Replace the NPC's dialog. */
			Npclist[UserList[UserIndex].flags.TargetNPC].desc = NewDialog;
		}
	}
}

/* '' */
/* ' Handles the "Impersonate" message. */
/* ' */


void HoAClientPacketHandler::handleImpersonate(Impersonate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 20/11/2010 */
	/* ' */
	/* '*************************************************** */

	/* ' Dsgm/Dsrm/Rm */
	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		return;
	}

	int NpcIndex;
	NpcIndex = UserList[UserIndex].flags.TargetNPC;

	if (NpcIndex == 0) {
		return;
	}

	/* ' Copy head, body and desc */
	ImitateNpc(UserIndex, NpcIndex);

	/* ' Teleports user to npc's coords */
	WarpUserChar(UserIndex, Npclist[NpcIndex].Pos.Map, Npclist[NpcIndex].Pos.X, Npclist[NpcIndex].Pos.Y,
			false, true);

	/* ' Log gm */
	LogGM(UserList[UserIndex].Name,
			"/IMPERSONAR con " + Npclist[NpcIndex].Name + " en mapa " + vb6::CStr(UserList[UserIndex].Pos.Map));

	/* ' Remove npc */
	QuitarNPC(NpcIndex);
}

/* '' */
/* ' Handles the "Imitate" message. */
/* ' */


void HoAClientPacketHandler::handleImitate(Imitate* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: ZaMa */
	/* 'Last Modification: 20/11/2010 */
	/* ' */
	/* '*************************************************** */

	/* ' Dsgm/Dsrm/Rm/ConseRm */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios, PlayerType_SemiDios)) {
		return;
	}

	int NpcIndex;
	NpcIndex = UserList[UserIndex].flags.TargetNPC;

	if (NpcIndex == 0) {
		return;
	}

	/* ' Copy head, body and desc */
	ImitateNpc(UserIndex, NpcIndex);
	LogGM(UserList[UserIndex].Name,
			"/MIMETIZAR con " + Npclist[NpcIndex].Name + " en mapa " + vb6::CStr(UserList[UserIndex].Pos.Map));

}

/* '' */
/* ' Handles the "RecordAdd" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message */

void HoAClientPacketHandler::handleRecordAdd(RecordAdd* p) { (void)p;
	/* '************************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modify Date: 29/11/2010 */
	/* ' */
	/* '************************************************************** */

	std::string UserName;
	std::string Reason;

	UserName = p->UserName;
	Reason = p->Reason;

	if (!(UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster))) {
		/* 'Verificamos que exista el personaje */
		if (!FileExist(GetCharPath(UserName))) {
			WriteShowMessageBox(UserIndex, "El personaje no existe");
		} else {
			/* 'Agregamos el seguimiento */
			AddRecord(UserIndex, UserName, Reason);

			/* 'Enviamos la nueva lista de personajes */
			WriteRecordList(UserIndex);
		}
	}
}

/* '' */
/* ' Handles the "RecordAddObs" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message. */

void HoAClientPacketHandler::handleRecordAddObs(RecordAddObs* p) { (void)p;
	/* '************************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modify Date: 29/11/2010 */
	/* ' */
	/* '************************************************************** */

	int RecordIndex;
	std::string& Obs = p->Obs;

	RecordIndex = p->Index;

	if (!(UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster))) {
		/* 'Agregamos la observación */
		AddObs(UserIndex, RecordIndex, Obs);

		/* 'Actualizamos la información */
		WriteRecordDetails(UserIndex, RecordIndex);
	}
}

/* '' */
/* ' Handles the "RecordRemove" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message. */

void HoAClientPacketHandler::handleRecordRemove(RecordRemove* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modification: 29/11/2010 */
	/* ' */
	/* '*************************************************** */
	int RecordIndex;

	RecordIndex = p->Index;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	/* 'Sólo dioses pueden remover los seguimientos, los otros reciben una advertencia: */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios)) {
		RemoveRecord(RecordIndex);
		WriteShowMessageBox(UserIndex, "Se ha eliminado el seguimiento.");
		WriteRecordList(UserIndex);
	} else {
		WriteShowMessageBox(UserIndex, "Sólo los dioses pueden eliminar seguimientos.");
	}
}

/* '' */
/* ' Handles the "RecordListRequest" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message. */

void HoAClientPacketHandler::handleRecordListRequest(RecordListRequest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modification: 29/11/2010 */
	/* ' */
	/* '*************************************************** */



	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	WriteRecordList(UserIndex);
}

/* '' */
/* ' Handles the "RecordDetailsRequest" message. */
/* ' */
/* ' @param UserIndex The index of the user sending the message. */

void HoAClientPacketHandler::handleRecordDetailsRequest(RecordDetailsRequest* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Amraphen */
	/* 'Last Modification: 07/04/2011 */
	/* 'Handles the "RecordListRequest" message */
	/* '*************************************************** */
	int RecordIndex;

	RecordIndex = p->Index;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_RoleMaster)) {
		return;
	}

	WriteRecordDetails(UserIndex, RecordIndex);
}

void HoAClientPacketHandler::handleMoveItem(MoveItem* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Ignacio Mariano Tirabasso (Budi) */
	/* 'Last Modification: 01/01/2011 */
	/* ' */
	/* '*************************************************** */

	moveItem(UserIndex, p->OldSlot, p->NewSlot);
}

/* '' */
/* ' Handles the "HigherAdminsMessage" message. */
/* ' */


void HoAClientPacketHandler::handleHigherAdminsMessage(HigherAdminsMessage* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Torres Patricio (Pato) */
	/* 'Last Modification: 03/30/12 */
	/* ' */
	/* '*************************************************** */

	std::string& message = p->Message;

	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		LogGM(UserList[UserIndex].Name, "Mensaje a Dioses:" + message);

		if (vb6::LenB(message) != 0) {
			/* 'Analize chat... */
			ParseChat(message);

			SendData(SendTarget_ToHigherAdminsButRMs, 0,
					hoa::protocol::server::BuildConsoleMsg(UserList[UserIndex].Name + "(Sólo Dioses)> " + message,
							FontTypeNames_FONTTYPE_GMMSG));
		}
	}

}

/* '' */
/* ' Handle the "AlterName" message */
/* ' */
/* ' @param userIndex The index of the user sending the message */

void HoAClientPacketHandler::handleAlterGuildName(AlterGuildName* p) { (void)p;
	/* '*************************************************** */
	/* 'Author: Lex! */
	/* 'Last Modification: 14/05/12 */
	/* 'Change guild name */
	/* '*************************************************** */

	/* 'Reads the userName and newUser Packets */
	std::string GuildName;
	std::string newGuildName;
	int GuildIndex;

	GuildName = p->OldGuildName;
	newGuildName = p->NewGuildName;
	GuildName = vb6::Trim(GuildName);
	newGuildName = vb6::Trim(newGuildName);

	if (!UserTieneAlgunPrivilegios(UserIndex, PlayerType_RoleMaster) && UserTieneAlgunPrivilegios(UserIndex, PlayerType_Admin, PlayerType_Dios)) {
		if (vb6::LenB(GuildName) == 0 || vb6::LenB(newGuildName) == 0) {
			WriteConsoleMsg(UserIndex, "Usar: /ACLAN origen@destino", FontTypeNames_FONTTYPE_INFO);
		} else {
			/* 'Revisa si el nombre nuevo del clan existe */
			if ((vb6::InStrB(newGuildName, "+") != 0)) {
				newGuildName = vb6::Replace(newGuildName, "+", " ");
			}

			GuildIndex = GetGuildIndex(newGuildName);
			if (GuildIndex > 0) {
				WriteConsoleMsg(UserIndex, "El clan destino ya existe.", FontTypeNames_FONTTYPE_INFO);
			} else {
				/* 'Revisa si el nombre del clan existe */
				if ((vb6::InStrB(GuildName, "+") != 0)) {
					GuildName = vb6::Replace(GuildName, "+", " ");
				}

				GuildIndex = GetGuildIndex(GuildName);
				if (GuildIndex > 0) {
					/* ' Existe clan origen y no el de destino */
					/* ' Verifica si existen archivos del clan, los crea con nombre nuevo y borra los viejos */
					if (FileExist(GetGuildsPath(GuildName, EGUILDPATH::Members))) {
						vb6::FileCopy(GetGuildsPath(GuildName, EGUILDPATH::Members),
								GetGuildsPath(newGuildName, EGUILDPATH::Members));
						vb6::Kill(GetGuildsPath(GuildName, EGUILDPATH::Members));
					}

					if (FileExist(GetGuildsPath(GuildName, EGUILDPATH::Relaciones))) {
						vb6::FileCopy(GetGuildsPath(GuildName, EGUILDPATH::Relaciones),
								GetGuildsPath(newGuildName, EGUILDPATH::Relaciones));
						vb6::Kill(GetGuildsPath(GuildName, EGUILDPATH::Relaciones));
					}

					if (FileExist(GetGuildsPath(GuildName, EGUILDPATH::Propositions))) {
						vb6::FileCopy(GetGuildsPath(GuildName, EGUILDPATH::Propositions),
								GetGuildsPath(newGuildName, EGUILDPATH::Propositions));
						vb6::Kill(GetGuildsPath(GuildName, EGUILDPATH::Propositions));
					}

					if (FileExist(GetGuildsPath(GuildName, EGUILDPATH::Solicitudes))) {
						vb6::FileCopy(GetGuildsPath(GuildName, EGUILDPATH::Solicitudes),
								GetGuildsPath(newGuildName, EGUILDPATH::Solicitudes));
						vb6::Kill(GetGuildsPath(GuildName, EGUILDPATH::Solicitudes));
					}

					if (FileExist(GetGuildsPath(GuildName, EGUILDPATH::Votaciones))) {
						vb6::FileCopy(GetGuildsPath(GuildName, EGUILDPATH::Votaciones),
								GetGuildsPath(newGuildName, EGUILDPATH::Votaciones));
						vb6::Kill(GetGuildsPath(GuildName, EGUILDPATH::Votaciones));
					}

					/* ' Actualiza nombre del clan en guildsinfo y server */
					WriteVar(GetDatPath(DATPATH::GuildsInfo), "GUILD" + vb6::CStr(GuildIndex), "GuildName", newGuildName);
					SetNewGuildName(GuildIndex, newGuildName);

					/* ' Actualiza todos los online del clan */
					for (auto MemberIndex : guild_Iterador_ProximoUserIndex(GuildIndex)) {
						if ((UserIndexSocketValido(MemberIndex))) {
							RefreshCharStatus(MemberIndex);
						}
					}

					/* ' Avisa que sali? todo OK y guarda en log del GM */
					WriteConsoleMsg(UserIndex,
							"El clan " + GuildName + " fue renombrado como " + newGuildName,
							FontTypeNames_FONTTYPE_INFO);
					LogGM(UserList[UserIndex].Name,
							"Ha cambiado el nombre del clan " + GuildName + ". Ahora se llama "
									+ newGuildName);
				} else {
					WriteConsoleMsg(UserIndex, "El clan origen no existe.", FontTypeNames_FONTTYPE_INFO);
				}
			}
		}
	}



}
