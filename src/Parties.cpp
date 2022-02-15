/******************************************************************************
    Copyright (C) 2002-2015 Argentum Online & Dakara Online Developers

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

#include "Parties.h"

/* '' */
/* ' SOPORTES PARA LAS PARTIES */
/* ' (Ver este modulo como una clase abstracta "PartyManager") */
/* ' */

/* '' */
/* 'cantidad maxima de parties en el servidor */
const int MAX_PARTIES = 300;

/* '' */
/* 'nivel minimo para crear party */
const int MINPARTYLEVEL = 15;

/* '' */
/* 'Cantidad maxima de gente en la party */
const int PARTY_MAXMEMBERS = 5;

/* '' */
/* 'Si esto esta en True, la exp sale por cada golpe que le da */
/* 'Si no, la exp la recibe al salirse de la party (pq las partys, floodean) */
const bool PARTY_EXPERIENCIAPORGOLPE = false;

/* '' */
/* 'maxima diferencia de niveles permitida en una party */
const int MAXPARTYDELTALEVEL = 7;

/* '' */
/* 'distancia al leader para que este acepte el ingreso */
const int MAXDISTANCIAINGRESOPARTY = 2;

/* '' */
/* 'maxima distancia a un exito para obtener su experiencia */
const int PARTY_MAXDISTANCIA = 18;

/* '' */
/* 'restan las muertes de los miembros? */
const bool CASTIGOS = false;

/* '' */
/* 'Numero al que elevamos el nivel de cada miembro de la party */
/* 'Esto es usado para calcular la distribución de la experiencia entre los miembros */
/* 'Se lee del archivo de balance */
float ExponenteNivelParty;

/* '' */
/* 'tPartyMember */
/* ' */
/* ' @param UserIndex UserIndex */
/* ' @param Experiencia Experiencia */
/* ' */

int NextParty() {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	// FIXME Busqueda lineal!
	int i;
	retval = -1;
	for (i = (1); i <= (MAX_PARTIES); i++) {
		if (Parties[i].get() == nullptr) {
			retval = i;
			return retval;
		}
	}
	return retval;
}

bool PuedeCrearParty(int UserIndex) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: 05/22/2010 (Marco) */
	/* ' - 05/22/2010 : staff members aren't allowed to party anyone. (Marco) */
	/* '*************************************************** */

	retval = true;

	if (!UserTienePrivilegio(UserIndex, PlayerType_User)) {
		/* 'staff members aren't allowed to create parties. */
		WriteConsoleMsg(UserIndex, "Staff members can't create parties!",
				FontTypeNames_FONTTYPE_PARTY);
		retval = false;
	} else if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_PARTY);
		retval = false;
	}
	return retval;
}

void CrearParty(int UserIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int tInt;

	if (UserList[UserIndex].PartyIndex == 0) {
		if (UserList[UserIndex].flags.Muerto == 0) {
			tInt = NextParty();
			if (tInt == -1) {
				WriteConsoleMsg(UserIndex, "No more parties can be created at the time.",
						FontTypeNames_FONTTYPE_PARTY);
				return;
			} else {
				Parties[tInt].reset(new clsParty());
				if (!Parties[tInt]->NuevoMiembro(UserIndex)) {
					WriteConsoleMsg(UserIndex, "The party is full, you cannot join it.",
							FontTypeNames_FONTTYPE_PARTY);
					Parties[tInt].reset();
					return;
				} else {
					WriteConsoleMsg(UserIndex, "You've created a party!", FontTypeNames_FONTTYPE_PARTY);
					UserList[UserIndex].PartyIndex = tInt;
					UserList[UserIndex].PartySolicitud = 0;
					if (!Parties[tInt]->HacerLeader(UserIndex)) {
						WriteConsoleMsg(UserIndex, "You can't make yourself leader.",
								FontTypeNames_FONTTYPE_PARTY);
					} else {
						WriteConsoleMsg(UserIndex, "You've become the leader of your party!",
								FontTypeNames_FONTTYPE_PARTY);
					}
				}
			}
		} else {
			WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_PARTY);
		}
	} else {
		WriteConsoleMsg(UserIndex, "You already belong to a party.", FontTypeNames_FONTTYPE_PARTY);
	}
}

void SolicitarIngresoAParty(int UserIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: 05/22/2010 (Marco) */
	/* ' - 05/22/2010 : staff members aren't allowed to party anyone. (Marco) */
	/* '18/09/2010: ZaMa - Ahora le avisa al funda de la party cuando alguien quiere ingresar a la misma. */
	/* '18/09/2010: ZaMa - Contemple mas ecepciones (solo se le puede mandar party al lider) */
	/* '*************************************************** */

	/* 'ESTO ES enviado por el PJ para solicitar el ingreso a la party */
	int TargetUserIndex;
	int PartyIndex;

	/* 'staff members aren't allowed to party anyone */
	if (!UserTienePrivilegio(UserIndex, PlayerType_User)) {
		WriteConsoleMsg(UserIndex, "Staff members can't join parties!",
				FontTypeNames_FONTTYPE_PARTY);
		return;
	}

	if (UserList[UserIndex].PartyIndex > 0) {
		/* 'si ya esta en una party */
		WriteConsoleMsg(UserIndex, "You already belong to a party, type /LEAVEPARTY to leave it.",
				FontTypeNames_FONTTYPE_PARTY);
		UserList[UserIndex].PartySolicitud = 0;
		return;
	}

	/* ' Muerto? */
	if (UserList[UserIndex].flags.Muerto == 1) {
		WriteConsoleMsg(UserIndex, "You're dead!!", FontTypeNames_FONTTYPE_INFO);
		UserList[UserIndex].PartySolicitud = 0;
		return;
	}

	TargetUserIndex = UserList[UserIndex].flags.TargetUser;
	/* ' Target valido? */
	if (TargetUserIndex > 0) {

		PartyIndex = UserList[TargetUserIndex].PartyIndex;
		/* ' Tiene party? */
		if (PartyIndex > 0) {

			/* ' Es el lider? */
			if (Parties[PartyIndex]->EsPartyLeader(TargetUserIndex)) {
				UserList[UserIndex].PartySolicitud = PartyIndex;
				WriteConsoleMsg(UserIndex, "The leader will decide if you are accepted in the party.",
						FontTypeNames_FONTTYPE_PARTY);
				WriteConsoleMsg(TargetUserIndex, UserList[UserIndex].Name + " is requesting admission to your party.",
						FontTypeNames_FONTTYPE_PARTY);

				/* ' No es lider */
			} else {
				WriteConsoleMsg(UserIndex, UserList[TargetUserIndex].Name + " is not the party's leader.",
						FontTypeNames_FONTTYPE_PARTY);
			}

			/* ' No tiene party */
		} else {
			WriteConsoleMsg(UserIndex, UserList[TargetUserIndex].Name + " doesn't belong to any party.",
					FontTypeNames_FONTTYPE_PARTY);
			UserList[UserIndex].PartySolicitud = 0;
			return;
		}

		/* ' Target inválido */
	} else {
		WriteConsoleMsg(UserIndex,
				"To join a party you must first click the founder and then type /PARTY",
				FontTypeNames_FONTTYPE_PARTY);
		UserList[UserIndex].PartySolicitud = 0;
	}

}

void SalirDeParty(int UserIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int PI;
	PI = UserList[UserIndex].PartyIndex;
	if (PI > 0) {
		if (Parties[PI]->SaleMiembro(UserIndex)) {
			/* 'sale el leader */
			Parties[PI].reset();
		} else {
			UserList[UserIndex].PartyIndex = 0;
		}
	} else {
		WriteConsoleMsg(UserIndex, "You are not a member of any party.", FontTypeNames_FONTTYPE_INFO);
	}

}

void ExpulsarDeParty(int leader, int OldMember) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int PI;
	PI = UserList[leader].PartyIndex;

	if (PI == UserList[OldMember].PartyIndex) {
		if (Parties[PI]->SaleMiembro(OldMember)) {
			/* 'si la funcion me da true, entonces la party se disolvio */
			/* 'y los partyindex fueron reseteados a 0 */
			Parties[PI].reset();
		} else {
			UserList[OldMember].PartyIndex = 0;
		}
	} else {
		WriteConsoleMsg(leader, vb6::LCase(UserList[OldMember].Name) + " does not belong to your party",
				FontTypeNames_FONTTYPE_INFO);
	}

}

/* '' */
/* ' Determines if a user can use party commands like /acceptparty or not. */
/* ' */
/* ' @param User Specifies reference to user */
/* ' @return  True if the user can use party commands, false if not. */
bool UserPuedeEjecutarComandos(int User) {
	bool retval = false;
	/* '************************************************* */
	/* 'Author: Marco Vanotti(Marco) */
	/* 'Last modified: 05/05/09 */
	/* ' */
	/* '************************************************* */
	int PI;

	PI = UserList[User].PartyIndex;

	if (PI > 0) {
		if (Parties[PI]->EsPartyLeader(User)) {
			retval = true;
		} else {
			WriteConsoleMsg(User, "You are not the leader of your party!", FontTypeNames_FONTTYPE_PARTY);
			return retval;
		}
	} else {
		WriteConsoleMsg(User, "You're not a member of any party.", FontTypeNames_FONTTYPE_INFO);
		return retval;
	}
	return retval;
}

void AprobarIngresoAParty(int leader, int NewMember) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: 11/03/2010 */
	/* '11/03/2010: ZaMa - Le avisa al lider si intenta aceptar a alguien que sea mimebro de su propia party. */
	/* '*************************************************** */

	/* 'el UI es el leader */
	int PI;
	std::string razon;

	PI = UserList[leader].PartyIndex;

	if (UserList[NewMember].PartySolicitud == PI) {
		if (UserList[NewMember].flags.Muerto != 1) {
			if (UserList[NewMember].PartyIndex == 0) {
				if (Parties[PI]->PuedeEntrar(NewMember, razon)) {
					if (Parties[PI]->NuevoMiembro(NewMember)) {
						Parties[PI]->MandarMensajeAConsola(
								UserList[leader].Name + " has accepted " + UserList[NewMember].Name
										+ " as a party member.", "Server");
						UserList[NewMember].PartyIndex = PI;
						UserList[NewMember].PartySolicitud = 0;
					} else {
						/* 'no pudo entrar */
						/* 'ACA UNO PUEDE CODIFICAR OTRO TIPO DE ERRORES... */
						SendData(SendTarget_ToAdmins, leader,
								dakara::protocol::server::BuildConsoleMsg(
										" Server> CATÁSTROFE EN PARTIES, NUEVOMIEMBRO DIO FALSE! :S ",
										FontTypeNames_FONTTYPE_PARTY));
					}
				} else {
					/* 'no debe entrar */
					WriteConsoleMsg(leader, razon, FontTypeNames_FONTTYPE_PARTY);
				}
			} else {
				if (UserList[NewMember].PartyIndex == PI) {
					WriteConsoleMsg(leader,
							vb6::LCase(UserList[NewMember].Name) + " is already a party member.",
							FontTypeNames_FONTTYPE_PARTY);
				} else {
					WriteConsoleMsg(leader, UserList[NewMember].Name + " is already a member of another party.",
							FontTypeNames_FONTTYPE_PARTY);
				}

				return;
			}
		} else {
			WriteConsoleMsg(leader, "You are dead!, you can't accept members in this state!",
					FontTypeNames_FONTTYPE_PARTY);
			return;
		}
	} else {
		if (UserList[NewMember].PartyIndex == PI) {
			WriteConsoleMsg(leader, vb6::LCase(UserList[NewMember].Name) + " is already a party member.",
					FontTypeNames_FONTTYPE_PARTY);
		} else {
			WriteConsoleMsg(leader,
					vb6::LCase(UserList[NewMember].Name) + " has not requested being part of your party.",
					FontTypeNames_FONTTYPE_PARTY);
		}

		return;
	}

}

//int IsPartyMember(int UserIndex, int PartyIndex) {
//	int retval;
//	int MemberIndex;
//
//	for (MemberIndex = (1); MemberIndex <= (PARTY_MAXMEMBERS); MemberIndex++) {
//
//	}
//	return retval;
//}

void BroadCastParty(int UserIndex, std::string & texto) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int PI;

	PI = UserList[UserIndex].PartyIndex;

	if (PI > 0) {
		Parties[PI]->MandarMensajeAConsola(texto, UserList[UserIndex].Name);
	}

}

void OnlineParty(int UserIndex) {
	/* '************************************************* */
	/* 'Author: Unknown */
	/* 'Last modified: 11/27/09 (Budi) */
	/* 'Adapte la función a los nuevos métodos de clsParty */
	/* '************************************************* */
	int i;
	int PI;
	std::string Text;
	std::vector<int> MembersOnline;

	PI = UserList[UserIndex].PartyIndex;

	if (PI > 0) {
		Parties[PI]->ObtenerMiembrosOnline(MembersOnline);
		Text = "Name(Exp): ";
		for (i = (0); i < (int) MembersOnline.size(); i++) {
			if (MembersOnline[i] > 0) {
				Text = Text + " - " + UserList[MembersOnline[i]].Name + " ("
						+ vb6::CStr(vb6::Fix(Parties[PI]->MiExperiencia(MembersOnline[i]))) + ")";
			}
		}
		Text = Text + ". Total XP: " + vb6::CStr(Parties[PI]->ObtenerExperienciaTotal());
		WriteConsoleMsg(UserIndex, Text, FontTypeNames_FONTTYPE_PARTY);
	}

}

void TransformarEnLider(int OldLeader, int NewLeader) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int PI;

	if (OldLeader == NewLeader) {
		return;
	}

	PI = UserList[OldLeader].PartyIndex;

	if (PI == UserList[NewLeader].PartyIndex) {
		if (UserList[NewLeader].flags.Muerto == 0) {
			if (Parties[PI]->HacerLeader(NewLeader)) {
				Parties[PI]->MandarMensajeAConsola(
						"The new leader of the party is " + UserList[NewLeader].Name,
						UserList[OldLeader].Name);
			} else {
				WriteConsoleMsg(OldLeader, "The command switch has not been made!",
						FontTypeNames_FONTTYPE_PARTY);
			}
		} else {
			WriteConsoleMsg(OldLeader, "is dead!", FontTypeNames_FONTTYPE_INFO);
		}
	} else {
		WriteConsoleMsg(OldLeader, vb6::LCase(UserList[NewLeader].Name) + " does not belong to your party",
				FontTypeNames_FONTTYPE_INFO);
	}

}

void ActualizaExperiencias() {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'esta funcion se invoca antes de worlsaves, y apagar servidores */
	/* 'en caso que la experiencia sea acumulada y no por golpe */
	/* 'para que grabe los datos en los charfiles */
	int i;

	if (!PARTY_EXPERIENCIAPORGOLPE) {

		haciendoBK = true;
		SendData(SendTarget_ToAll, 0,
				dakara::protocol::server::BuildPauseToggle());

		SendData(SendTarget_ToAll, 0,
				dakara::protocol::server::BuildConsoleMsg("Server> Distributing XP among parties.",
						FontTypeNames_FONTTYPE_SERVER));
		for (i = (1); i <= (MAX_PARTIES); i++) {
			if (Parties[i] != nullptr) {
				Parties[i]->FlushExperiencia();
			}
		}
		SendData(SendTarget_ToAll, 0,
				dakara::protocol::server::BuildConsoleMsg("Server> XP distributed.",
						FontTypeNames_FONTTYPE_SERVER));
		SendData(SendTarget_ToAll, 0,
				dakara::protocol::server::BuildPauseToggle());
		haciendoBK = false;

	}

}

void ObtenerExito(int UserIndex, int Exp, int mapa, int X, int Y) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (Exp <= 0) {
		if (!CASTIGOS) {
			return;
		}
	}

	Parties[UserList[UserIndex].PartyIndex]->ObtenerExito(Exp, mapa, X, Y);

}

int CantMiembros(int UserIndex) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	retval = 0;
	if (UserList[UserIndex].PartyIndex > 0) {
		retval = Parties[UserList[UserIndex].PartyIndex]->CantMiembros();
	}

	return retval;
}

/* '' */
/* ' Sets the new p_sumaniveleselevados to the party. */
/* ' */
/* ' @param UserInidex Specifies reference to user */
/* ' @remarks When a user level up and he is in a party, we call this sub to don't desestabilice the party exp formula */
void ActualizarSumaNivelesElevados(int UserIndex) {
	/* '************************************************* */
	/* 'Author: Marco Vanotti (MarKoxX) */
	/* 'Last modified: 28/10/08 */
	/* ' */
	/* '************************************************* */
	if (UserList[UserIndex].PartyIndex > 0) {
		Parties[UserList[UserIndex].PartyIndex]->UpdateSumaNivelesElevados(UserList[UserIndex].Stats.ELV);
	}
}
