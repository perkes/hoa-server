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

#include "Clanes.h"

// static std::string GUILDINFOFILE;
/* 'archivo .\guilds\guildinfo.ini o similar */

static const int MAX_GUILDS = 2000;
/* 'cantidad maxima de guilds en el servidor */

int CANTIDADDECLANES;
/* 'cantidad actual de clanes en el servidor */

static vb6::array<std::shared_ptr<clsClan>> guilds;
/* 'array global de guilds, se indexa por userlist().guildindex */

static const int CANTIDADMAXIMACODEX = 8;
/* 'cantidad maxima de codecs que se pueden definir */

const int MAXASPIRANTES = 10;
/* 'cantidad maxima de aspirantes que puede tener un clan acumulados a la vez */

const int MAXCLANMEMBERS = 1000;

static const int MAXANTIFACCION = 5;
/* 'puntos maximos de antifaccion que un clan tolera antes de ser cambiada su alineacion */

/* 'alineaciones permitidas */

/* 'numero de .wav del cliente */

/* 'estado entre clanes */
/* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
/* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
/* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */

void LoadGuildsDB() {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	std::string CantClanes;
	int i;
	std::string TempStr;
	ALINEACION_GUILD Alin;

	std::string GUILDINFOFILE = GetDatPath(DATPATH::GuildsInfo);

	CantClanes = GetVar(GUILDINFOFILE, "INIT", "nroGuilds");

	if (vb6::IsNumeric(CantClanes)) {
		CANTIDADDECLANES = vb6::CInt(CantClanes);
	} else {
		CANTIDADDECLANES = 0;
	}

	CANTIDADDECLANES = vb6::Constrain(CANTIDADDECLANES, 0, MAX_GUILDS);

	guilds.redim(1, MAX_GUILDS); /* XXX */

	for (i = (1); i <= (CANTIDADDECLANES); i++) {
		guilds[i].reset(new clsClan());
		TempStr = GetVar(GUILDINFOFILE, "GUILD" + vb6::CStr(i), "GUILDNAME");
		Alin = String2Alineacion(GetVar(GUILDINFOFILE, "GUILD" + vb6::CStr(i), "Alineacion"));
		guilds[i]->Inicializar(TempStr, i, Alin);
	}

}

bool m_ConectarMiembroAClan(int UserIndex, int GuildIndex) {
	bool retval = false;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	bool NuevaA;
	std::string News;

	/* 'x las dudas... */
	if (GuildIndex > CANTIDADDECLANES || GuildIndex <= 0) {
		return retval;
	}
	if (m_EstadoPermiteEntrar(UserIndex, GuildIndex)) {
		guilds[GuildIndex]->ConectarMiembro(UserIndex);
		UserList[UserIndex].GuildIndex = GuildIndex;
		retval = true;
	} else {
		retval = m_ValidarPermanencia(UserIndex, true, NuevaA);
		if (NuevaA) {
			News = News + "The clan has a new alignment.";
		}
		/* 'If NuevoL Or NuevaA Then Call guilds(GuildIndex).SetGuildNews(News) */
	}

	return retval;
}

bool m_ValidarPermanencia(int UserIndex, bool SumaAntifaccion, bool & CambioAlineacion) {
	bool retval;
	/* '*************************************************** */
	/* 'Autor: Unknown (orginal version) */
	/* 'Last Modification: 14/12/2009 */
	/* '25/03/2009: ZaMa - Desequipo los items faccionarios que tenga el funda al abandonar la faccion */
	/* '14/12/2009: ZaMa - La alineacion del clan depende del lider */
	/* '14/02/2010: ZaMa - Ya no es necesario saber si el lider cambia, ya que no puede cambiar. */
	/* '*************************************************** */

	int GuildIndex;

	retval = true;

	GuildIndex = UserList[UserIndex].GuildIndex;
	if (GuildIndex > CANTIDADDECLANES && GuildIndex <= 0) {
		return retval;
	}

	if (!m_EstadoPermiteEntrar(UserIndex, GuildIndex)) {

		/* ' Es el lider, bajamos 1 rango de alineacion */
		if (m_EsGuildLeader(UserList[UserIndex].Name, GuildIndex)) {
			LogClanes(
					vb6::CStr(UserList[UserIndex].Name) + ", leader of " + guilds[GuildIndex]->GuildName()
							+ " reduced their clan's alignment.");

			CambioAlineacion = true;

			/* ' Por si paso de ser armada/legion a pk/ciuda, chequeo de nuevo */
			do {
				UpdateGuildMembers(GuildIndex);
			} while (!(m_EstadoPermiteEntrar(UserIndex, GuildIndex)));
		} else {
			LogClanes(
					vb6::CStr(UserList[UserIndex].Name) + " of " + guilds[GuildIndex]->GuildName()
							+ " has been expelled.");

			retval = false;
			if (SumaAntifaccion) {
				guilds[GuildIndex]->PuntosAntifaccion(guilds[GuildIndex]->PuntosAntifaccion() + 1);
			}

			CambioAlineacion = guilds[GuildIndex]->PuntosAntifaccion() == MAXANTIFACCION;

			LogClanes(
					vb6::CStr(UserList[UserIndex].Name) + " of " + guilds[GuildIndex]->GuildName()
							+ vb6::IIf(CambioAlineacion, " SI ", " NO ")
							+ "caused a change of alignment. MAXANT: " + vb6::CStr(CambioAlineacion));

			m_EcharMiembroDeClan(-1, UserList[UserIndex].Name);

			/* ' Llegamos a la maxima cantidad de antifacciones permitidas, bajamos un grado de alineación */
			if (CambioAlineacion) {
				UpdateGuildMembers(GuildIndex);
			}
		}
	}
	return retval;
}

void UpdateGuildMembers(int GuildIndex) {
	/* '*************************************************** */
	/* 'Autor: ZaMa */
	/* 'Last Modification: 14/01/2010 (ZaMa) */
	/* '14/01/2010: ZaMa - Pulo detalles en el funcionamiento general. */
	/* '*************************************************** */
	std::vector<std::string> GuildMembers;
	int TotalMembers;
	int MemberIndex;
	bool Sale;
	std::string MemberName;
	int UserIndex;
	int Reenlistadas;

	/* ' Si devuelve true, cambio a neutro y echamos a todos los que estén de mas, sino no echamos a nadie */
	/* 'ALINEACION_NEUTRO) */
	if (guilds[GuildIndex]->CambiarAlineacion(BajarGrado(GuildIndex))) {

		/* 'uso GetMemberList y no los iteradores pq voy a rajar gente y puedo alterar */
		/* 'internamente al iterador en el proceso */
		GuildMembers = guilds[GuildIndex]->GetMemberList();
		TotalMembers = vb6::UBound(GuildMembers);

		for (MemberIndex = (0); MemberIndex <= (TotalMembers); MemberIndex++) {
			MemberName = GuildMembers[MemberIndex];

			/* 'vamos a violar un poco de capas.. */
			UserIndex = NameIndex(MemberName);
			if (UserIndex > 0) {
				Sale = !m_EstadoPermiteEntrar(UserIndex, GuildIndex);
			} else {
				Sale = !m_EstadoPermiteEntrarChar(MemberName, GuildIndex);
			}

			if (Sale) {
				/* 'hay que sacarlo de las facciones */
				if (m_EsGuildLeader(MemberName, GuildIndex)) {

					if (UserIndex > 0) {
						if (UserList[UserIndex].Faccion.ArmadaReal != 0) {
							ExpulsarFaccionReal(UserIndex);
							/* ' No cuenta como reenlistada :p. */
							UserList[UserIndex].Faccion.Reenlistadas =
									UserList[UserIndex].Faccion.Reenlistadas - 1;
						} else if (UserList[UserIndex].Faccion.FuerzasCaos != 0) {
							ExpulsarFaccionCaos(UserIndex);
							/* ' No cuenta como reenlistada :p. */
							UserList[UserIndex].Faccion.Reenlistadas =
									UserList[UserIndex].Faccion.Reenlistadas - 1;
						}
					} else {
						if (FileExist(GetCharPath(MemberName))) {
							WriteVar(GetCharPath(MemberName), "FACCIONES", "EjercitoCaos", 0);
							WriteVar(GetCharPath(MemberName), "FACCIONES", "EjercitoReal", 0);
							Reenlistadas = vb6::CInt(GetVar(GetCharPath(MemberName), "FACCIONES",
									"Reenlistadas"));
							WriteVar(GetCharPath(MemberName), "FACCIONES", "Reenlistadas",
									vb6::IIf(Reenlistadas > 1, Reenlistadas - 1, Reenlistadas));
						}
					}
					/* 'sale si no es guildLeader */
				} else {
					m_EcharMiembroDeClan(-1, MemberName);
				}
			}
		}
	} else {
		/* ' Resetea los puntos de antifacción */
		guilds[GuildIndex]->PuntosAntifaccion(0);
	}
}

ALINEACION_GUILD BajarGrado(int GuildIndex) {
	ALINEACION_GUILD retval;
	/* '*************************************************** */
	/* 'Autor: ZaMa */
	/* 'Last Modification: 27/11/2009 */
	/* 'Reduce el grado de la alineacion a partir de la alineacion dada */
	/* '*************************************************** */

	switch (guilds[GuildIndex]->Alineacion()) {
	case ALINEACION_GUILD_ALINEACION_ARMADA:
		retval = ALINEACION_GUILD_ALINEACION_CIUDA;
		break;

	case ALINEACION_GUILD_ALINEACION_LEGION:
		retval = ALINEACION_GUILD_ALINEACION_CRIMINAL;
		break;

	default:
		retval = ALINEACION_GUILD_ALINEACION_NEUTRO;
		break;
	}

	return retval;
}

void m_DesconectarMiembroDelClan(int UserIndex, int GuildIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (UserList[UserIndex].GuildIndex > CANTIDADDECLANES) {
		return;
	}
	guilds[GuildIndex]->DesConectarMiembro(UserIndex);
}

bool m_EsGuildLeader(const std::string & PJ, int GuildIndex) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	retval = (vb6::UCase(PJ) == vb6::UCase(vb6::Trim(guilds[GuildIndex]->GetLeader())));
	return retval;
}

bool m_EsGuildFounder(const std::string & PJ, int GuildIndex) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	retval = (vb6::UCase(PJ) == vb6::UCase(vb6::Trim(guilds[GuildIndex]->Fundador())));
	return retval;
}

int m_EcharMiembroDeClan(int Expulsador, std::string Expulsado) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'UI echa a Expulsado del clan de Expulsado */
	int UserIndex;
	int GI;

	retval = 0;

	UserIndex = NameIndex(Expulsado);
	if (UserIndex > 0) {
		/* 'pj online */
		GI = UserList[UserIndex].GuildIndex;
		if (GI > 0) {
			if (m_PuedeSalirDeClan(Expulsado, GI, Expulsador)) {
				guilds[GI]->DesConectarMiembro(UserIndex);
				guilds[GI]->ExpulsarMiembro(Expulsado);
				LogClanes(
						vb6::CStr(Expulsado) + " has been expelled from " + guilds[GI]->GuildName()
								+ " by " + vb6::CStr(Expulsador));
				UserList[UserIndex].GuildIndex = 0;
				RefreshCharStatus(UserIndex);
				retval = GI;
			} else {
				retval = 0;
			}
		} else {
			retval = 0;
		}
	} else {
		/* 'pj offline */
		GI = GetGuildIndexFromChar(Expulsado);
		if (GI > 0) {
			if (m_PuedeSalirDeClan(Expulsado, GI, Expulsador)) {
				guilds[GI]->ExpulsarMiembro(Expulsado);
				LogClanes(
						vb6::CStr(Expulsado) + " has been expelled from " + guilds[GI]->GuildName()
								+ " by " + vb6::CStr(Expulsador));
				retval = GI;
			} else {
				retval = 0;
			}
		} else {
			retval = 0;
		}
	}

	return retval;
}

void ActualizarWebSite(int UserIndex, const std::string & Web) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		return;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		return;
	}

	guilds[GI]->SetURL(Web);

}

void ChangeCodexAndDesc(std::string & desc, std::vector<std::string> & codex, int GuildIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int i;

	if (GuildIndex < 1 || GuildIndex > CANTIDADDECLANES) {
		return;
	}

	guilds[GuildIndex]->SetDesc(desc);

	for (i = (0); i <= (vb6::UBound(codex)); i++) {
		guilds[GuildIndex]->SetCodex(i+1, codex[i]);
	}
}

void ActualizarNoticias(int UserIndex, std::string Datos) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: 21/02/2010 */
	/* '21/02/2010: ZaMa - Ahora le avisa a los miembros que cambio el guildnews. */
	/* '*************************************************** */

	int GI;

	GI = UserList[UserIndex].GuildIndex;

	if (GI <= 0 || GI > CANTIDADDECLANES) {
		return;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		return;
	}

	guilds[GI]->SetGuildNews(Datos);

	SendData(SendTarget_ToDiosesYclan, UserList[UserIndex].GuildIndex,
			hoa::protocol::server::BuildGuildChat(UserList[UserIndex].Name + " has updated the clan's news section!"));
}

bool CrearNuevoClan(int FundadorIndex, std::string & desc, std::string & GuildName, std::string & URL,
		std::vector<std::string> & codex, ALINEACION_GUILD Alineacion, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int CantCodex;
	int i;
	std::string DummyString;

	retval = false;
	if (!PuedeFundarUnClan(FundadorIndex, Alineacion, DummyString)) {
		refError = DummyString;
		return retval;
	}

	if (GuildName == "" || !GuildNameValido(GuildName)) {
		refError = "Invalid clan name.";
		return retval;
	}

	if (YaExiste(GuildName)) {
		refError = "A clan with that name already exists.";
		return retval;
	}

	CantCodex = vb6::UBound(codex) + 1;
	/* 'tenemos todo para fundar ya */
	if (CANTIDADDECLANES < vb6::UBound(guilds)) {
		CANTIDADDECLANES = CANTIDADDECLANES + 1;
		/* 'ReDim Preserve Guilds(1 To CANTIDADDECLANES) As clsClan */

		/* 'constructor custom de la clase clan */
		guilds[CANTIDADDECLANES].reset(new clsClan());
		guilds[CANTIDADDECLANES]->Inicializar(GuildName, CANTIDADDECLANES, Alineacion);
		/* 'Damos de alta al clan como nuevo inicializando sus archivos */
		guilds[CANTIDADDECLANES]->InicializarNuevoClan(UserList[FundadorIndex].Name);
		/* 'seteamos codex y descripcion */
		for (i = (1); i <= (CantCodex); i++) {
			guilds[CANTIDADDECLANES]->SetCodex(i, codex[i - 1]);
		}
		
		guilds[CANTIDADDECLANES]->SetDesc(desc);
		
		guilds[CANTIDADDECLANES]->SetGuildNews(
				"Clan created with alignment: " + Alineacion2String(Alineacion));
		
		guilds[CANTIDADDECLANES]->SetLeader(UserList[FundadorIndex].Name);
		guilds[CANTIDADDECLANES]->SetURL(URL);

		/* '"conectamos" al nuevo miembro a la lista de la clase */
		guilds[CANTIDADDECLANES]->AceptarNuevoMiembro(UserList[FundadorIndex].Name);
		guilds[CANTIDADDECLANES]->ConectarMiembro(FundadorIndex);
		UserList[FundadorIndex].GuildIndex = CANTIDADDECLANES;
		RefreshCharStatus(FundadorIndex);
		
		for (i = (1); i <= (CANTIDADDECLANES - 1); i++) {
			guilds[i]->ProcesarFundacionDeOtroClan();
		}
	} else {
		refError = "No more clans can be created. Talk to an admin.";
		return retval;
	}

	retval = true;
	return retval;
}

void SendGuildNews(int UserIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GuildIndex;
	int i;

	GuildIndex = UserList[UserIndex].GuildIndex;
	if (GuildIndex == 0) {
		return;
	}

	std::vector<std::string> enemies;
	std::vector<std::string> allies;

	enemies.reserve(guilds[GuildIndex]->CantidadEnemys());
	allies.reserve(guilds[GuildIndex]->CantidadAllies());

	{
		auto & v = guilds[GuildIndex]->Iterador_ProximaRelacion();
		/* FIXME: Esto esta todo mal... el vector empieza en...? deberia ser un array */
		for (i = 1; i < (int)v.size(); ++i) {
			if (v[i] == RELACIONES_GUILD_ALIADOS) {
				allies.push_back(guilds[i]->GuildName());
			} else if (v[i] == RELACIONES_GUILD_GUERRA) {
				enemies.push_back(guilds[i]->GuildName());
			}
		}
	}

	WriteGuildNews(UserIndex, guilds[GuildIndex]->GetGuildNews(), enemies, allies);

	if (guilds[GuildIndex]->EleccionesAbiertas()) {
		WriteConsoleMsg(UserIndex, "Today is the vote to pick a new clan leader.",
				FontTypeNames_FONTTYPE_GUILD);
		WriteConsoleMsg(UserIndex,
				"The election will last 24 hours, any clan member can be voted for.",
				FontTypeNames_FONTTYPE_GUILD);
		WriteConsoleMsg(UserIndex, "To vote write /VOTE NICKNAME.", FontTypeNames_FONTTYPE_GUILD);
		WriteConsoleMsg(UserIndex, "Only one vote per clan member will be computed. Your vote cannot be changed.",
				FontTypeNames_FONTTYPE_GUILD);
	}

}

bool m_PuedeSalirDeClan(std::string & Nombre, int GuildIndex, int QuienLoEchaUI) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'sale solo si no es fundador del clan. */

	retval = false;
	if (GuildIndex == 0) {
		return retval;
	}

	/* 'esto es un parche, si viene en -1 es porque la invoca la rutina de expulsion automatica de clanes x antifacciones */
	if (QuienLoEchaUI == -1) {
		retval = true;
		return retval;
	}

	/* 'cuando UI no puede echar a nombre? */
	/* 'si no es gm Y no es lider del clan del pj Y no es el mismo que se va voluntariamente */
	if (UserTienePrivilegio(QuienLoEchaUI, PlayerType_User)) {
		if (!m_EsGuildLeader(vb6::UCase(UserList[QuienLoEchaUI].Name), GuildIndex)) {
			/* 'si no sale voluntariamente... */
			if (vb6::UCase(UserList[QuienLoEchaUI].Name) != vb6::UCase(Nombre)) {
				return retval;
			}
		}
	}

	/* ' Ahora el lider es el unico que no puede salir del clan */
	retval = vb6::UCase(guilds[GuildIndex]->GetLeader()) != vb6::UCase(Nombre);

	return retval;
}

bool PuedeFundarUnClan(int UserIndex, ALINEACION_GUILD Alineacion, std::string & refError) {
	bool retval = false;
	/* '*************************************************** */
	/* 'Autor: Unknown */
	/* 'Last Modification: 27/11/2009 */
	/* 'Returns true if can Found a guild */
	/* '27/11/2009: ZaMa - Ahora valida si ya fundo clan o no. */
	/* '*************************************************** */

	if (UserList[UserIndex].GuildIndex > 0) {
		refError = "You already belong to a clan, you can't found another one.";
		return retval;
	}

	if (UserList[UserIndex].Stats.ELV < 30) {
		refError = "In order to found a clan you must be at least level 30.";
		return retval;
	}

	switch (Alineacion) {
	case ALINEACION_GUILD_Null:
		throw std::runtime_error("Alineacion == ALINEACION_GUILD_Null en PuedeFundarUnClan");

	case ALINEACION_GUILD_ALINEACION_ARMADA:
		if (UserList[UserIndex].Faccion.ArmadaReal != 1) {
			refError = "In order to found a royal clan you must be a member of the Royal Army.";
			return retval;
		}
		break;

	case ALINEACION_GUILD_ALINEACION_CIUDA:
		if (criminal(UserIndex)) {
			refError = "In order to found a citizens clan you must not be a criminal.";
			return retval;
		}
		break;

	case ALINEACION_GUILD_ALINEACION_CRIMINAL:
		if (!criminal(UserIndex)) {
			refError = "In order to found a criminal clan you must not be a citizen.";
			return retval;
		}
		break;

	case ALINEACION_GUILD_ALINEACION_LEGION:
		if (UserList[UserIndex].Faccion.FuerzasCaos != 1) {
			refError = "In order to found an evil clan you must be a member of the Dark Legion.";
			return retval;
		}
		break;

	case ALINEACION_GUILD_ALINEACION_MASTER:
		if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_User, PlayerType_Consejero, PlayerType_SemiDios)) {
			refError = "In order to found a clan without any alignment you must be an admin.";
			return retval;
		}
		break;

	case ALINEACION_GUILD_ALINEACION_NEUTRO:
		if (UserList[UserIndex].Faccion.ArmadaReal != 0 || UserList[UserIndex].Faccion.FuerzasCaos != 0) {
			refError = "In order to found a neutral clan you must not belong to any faction.";
			return retval;
		}
		break;
	}

	retval = true;

	return retval;
}

bool m_EstadoPermiteEntrarChar(std::string & Personaje, int GuildIndex) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int Promedio = 0;
	int ELV = 0;
	int f = 0;

	retval = false;

	if (vb6::InStrB(Personaje, "/") != 0) {
		Personaje = vb6::Replace(Personaje, "/", "");
	}
	if (vb6::InStrB(Personaje, "/") != 0) {
		Personaje = vb6::Replace(Personaje, "/", "");
	}
	if (vb6::InStrB(Personaje, ".") != 0) {
		Personaje = vb6::Replace(Personaje, ".", "");
	}

	if (FileExist(GetCharPath(Personaje))) {
		Promedio = vb6::CLng(GetVar(GetCharPath(Personaje), "REP", "Promedio"));
		switch (guilds[GuildIndex]->Alineacion()) {
		case ALINEACION_GUILD_ALINEACION_ARMADA:
			if (Promedio >= 0) {
				ELV = vb6::CInt(GetVar(GetCharPath(Personaje), "Stats", "ELV"));
				if (ELV >= 25) {
					f = vb6::CByte(GetVar(GetCharPath(Personaje), "Facciones", "EjercitoReal"));
				}
				retval = vb6::IIf(ELV >= 25, f != 0, true);
			}
			break;

		case ALINEACION_GUILD_ALINEACION_CIUDA:
			retval = Promedio >= 0;
			break;

		case ALINEACION_GUILD_ALINEACION_CRIMINAL:
			retval = Promedio < 0;
			break;

		case ALINEACION_GUILD_ALINEACION_NEUTRO:
			retval = vb6::CByte(GetVar(GetCharPath(Personaje), "Facciones", "EjercitoReal")) == 0;
			retval = retval
					&& (vb6::CByte(GetVar(GetCharPath(Personaje), "Facciones", "EjercitoCaos")) == 0);
			break;

		case ALINEACION_GUILD_ALINEACION_LEGION:
			if (Promedio < 0) {
				ELV = vb6::CInt(GetVar(GetCharPath(Personaje), "Stats", "ELV"));
				if (ELV >= 25) {
					f = vb6::CByte(GetVar(GetCharPath(Personaje), "Facciones", "EjercitoCaos"));
				}
				retval = vb6::IIf(ELV >= 25, f != 0, true);
			}
			break;

		default:
			retval = true;
			break;
		}
	}
	return retval;
}

bool m_EstadoPermiteEntrar(int UserIndex, int GuildIndex) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	switch (guilds[GuildIndex]->Alineacion()) {
	case ALINEACION_GUILD_ALINEACION_ARMADA:
		retval = !criminal(UserIndex)
				&& vb6::IIf(UserList[UserIndex].Stats.ELV >= 25, UserList[UserIndex].Faccion.ArmadaReal != 0,
						true);

		break;

	case ALINEACION_GUILD_ALINEACION_LEGION:
		retval = criminal(UserIndex)
				&& vb6::IIf(UserList[UserIndex].Stats.ELV >= 25, UserList[UserIndex].Faccion.FuerzasCaos != 0,
						true);

		break;

	case ALINEACION_GUILD_ALINEACION_NEUTRO:
		retval = UserList[UserIndex].Faccion.ArmadaReal == 0 && UserList[UserIndex].Faccion.FuerzasCaos == 0;

		break;

	case ALINEACION_GUILD_ALINEACION_CIUDA:
		retval = !criminal(UserIndex);

		break;

	case ALINEACION_GUILD_ALINEACION_CRIMINAL:
		retval = criminal(UserIndex);

		/* 'game masters */
		break;

	default:
		retval = true;
		break;
	}
	return retval;
}

ALINEACION_GUILD String2Alineacion(std::string S) {
	ALINEACION_GUILD retval = ALINEACION_GUILD_Null;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (S == "Neutral") {
		retval = ALINEACION_GUILD_ALINEACION_NEUTRO;
	} else if (S ==  "Del Mal") {
		retval = ALINEACION_GUILD_ALINEACION_LEGION;
	} else if (S ==  "Real") {
		retval = ALINEACION_GUILD_ALINEACION_ARMADA;
	} else if (S ==  "Game Masters") {
		retval = ALINEACION_GUILD_ALINEACION_MASTER;
	} else if (S ==  "Legal") {
		retval = ALINEACION_GUILD_ALINEACION_CIUDA;
	} else if (S ==  "Criminal") {
		retval = ALINEACION_GUILD_ALINEACION_CRIMINAL;
	}

	return retval;
}

std::string Alineacion2String(ALINEACION_GUILD Alineacion) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	switch (Alineacion) {
	case ALINEACION_GUILD_Null:
		throw std::runtime_error("Alineacion == ALINEACION_GUILD_Null en Alineacion2String");

	case ALINEACION_GUILD_ALINEACION_NEUTRO:
		retval = "Neutral";
		break;

	case ALINEACION_GUILD_ALINEACION_LEGION:
		retval = "Del Mal";
		break;

	case ALINEACION_GUILD_ALINEACION_ARMADA:
		retval = "Real";
		break;

	case ALINEACION_GUILD_ALINEACION_MASTER:
		retval = "Game Masters";
		break;

	case ALINEACION_GUILD_ALINEACION_CIUDA:
		retval = "Legal";
		break;

	case ALINEACION_GUILD_ALINEACION_CRIMINAL:
		retval = "Criminal";
		break;
	}
	return retval;
}

std::string Relacion2String(RELACIONES_GUILD Relacion) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	switch (Relacion) {
	case RELACIONES_GUILD_ALIADOS:
		retval = "A";
		break;

	case RELACIONES_GUILD_GUERRA:
		retval = "G";
		break;

	case RELACIONES_GUILD_PAZ:
		retval = "P";
		break;

	default:
		retval = "?";
		break;
	}
	return retval;
}

RELACIONES_GUILD String2Relacion(std::string S) {
	RELACIONES_GUILD retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	std::string tmp = vb6::UCase(vb6::Trim(S));
	if (!tmp.size())
		return RELACIONES_GUILD_PAZ;

	switch (tmp[0]) {
	case 'P':
		retval = RELACIONES_GUILD_PAZ;
		break;

	case 'G':
		retval = RELACIONES_GUILD_GUERRA;
		break;

	case 'A':
		retval = RELACIONES_GUILD_ALIADOS;
		break;

	default:
		retval = RELACIONES_GUILD_PAZ;
		break;
	}
	return retval;
}

bool GuildNameValido(std::string cad) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int car;
	int i;

	/* 'old function by morgo */

	cad = vb6::LCase(cad);

	for (i = (1); i <= (int)(vb6::Len(cad)); i++) {
		car = vb6::Asc(vb6::Mid(cad, i, 1));

		if ((car < 97 || car > 122) && (car != 255) && (car != 32)) {
			retval = false;
			return retval;
		}

	}

	retval = true;

	return retval;
}

bool YaExiste(std::string GuildName) {
	bool retval = false;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int i;

	retval = false;
	GuildName = vb6::UCase(GuildName);

	for (i = (1); i <= (CANTIDADDECLANES); i++) {
		retval = (vb6::UCase(guilds[i]->GuildName()) == GuildName);
		if (retval) {
			return retval;
		}
	}

	return retval;
}

bool HasFound(std::string & UserName) {
	bool retval = false;
	/* '*************************************************** */
	/* 'Autor: ZaMa */
	/* 'Last Modification: 27/11/2009 */
	/* 'Returns true if it's already the founder of other guild */
	/* '*************************************************** */
	int i;
	std::string Name;

	Name = vb6::UCase(UserName);

	for (i = (1); i <= (CANTIDADDECLANES); i++) {
		retval = (vb6::UCase(guilds[i]->Fundador()) == Name);
		if (retval) {
			return retval;
		}
	}

	return retval;
}

bool v_AbrirElecciones(int UserIndex, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GuildIndex;

	retval = false;
	GuildIndex = UserList[UserIndex].GuildIndex;

	if (GuildIndex == 0 || GuildIndex > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GuildIndex)) {
		refError = "You are not the leader of your clan";
		return retval;
	}

	if (guilds[GuildIndex]->EleccionesAbiertas()) {
		refError = "Elections have already began.";
		return retval;
	}

	retval = true;
	guilds[GuildIndex]->AbrirElecciones();

	return retval;
}

bool v_UsuarioVota(int UserIndex, std::string & Votado, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GuildIndex;
	std::vector<std::string> list;
	int i;

	retval = false;
	GuildIndex = UserList[UserIndex].GuildIndex;

	if (GuildIndex == 0 || GuildIndex > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!guilds[GuildIndex]->EleccionesAbiertas()) {
		refError = "No elections are being held in your clan right now.";
		return retval;
	}

	list = guilds[GuildIndex]->GetMemberList();
	for (i = (0); i <= (vb6::UBound(list)); i++) {
		if (vb6::UCase(Votado) == list[i]) {
			break;
		}
	}

	if (i > vb6::UBound(list)) {
		refError = Votado + " does not belong to the clan.";
		return retval;
	}

	if (guilds[GuildIndex]->YaVoto(UserList[UserIndex].Name)) {
		refError = "You've already voted, you can't change your vote.";
		return retval;
	}

	guilds[GuildIndex]->ContabilizarVoto(UserList[UserIndex].Name, Votado);
	retval = true;

	return retval;
}

void v_RutinaElecciones() {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int i;

	SendData(SendTarget_ToAll, 0,
			hoa::protocol::server::BuildConsoleMsg("Server> Checking elections", FontTypeNames_FONTTYPE_SERVER));
	for (i = (1); i <= (CANTIDADDECLANES); i++) {
		if (guilds[i] == nullptr) {
			if (guilds[i]->RevisarElecciones()) {
				SendData(SendTarget_ToAll, 0,
						hoa::protocol::server::BuildConsoleMsg(
								"Server> " + guilds[i]->GetLeader() + " is the new leader of "
										+ guilds[i]->GuildName() + ".", FontTypeNames_FONTTYPE_SERVER));
			}
		}
		/* FIXME: proximo : */
	}
	SendData(SendTarget_ToAll, 0,
			hoa::protocol::server::BuildConsoleMsg("Server> elections checked.", FontTypeNames_FONTTYPE_SERVER));
}

int GetGuildIndexFromChar(std::string & PlayerName) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'aca si que vamos a violar las capas deliveradamente ya que */
	/* 'visual basic no permite declarar metodos de clase */
	std::string Temps;
	if (vb6::InStrB(PlayerName, "/") != 0) {
		PlayerName = vb6::Replace(PlayerName, "/", "");
	}
	if (vb6::InStrB(PlayerName, "/") != 0) {
		PlayerName = vb6::Replace(PlayerName, "/", "");
	}
	if (vb6::InStrB(PlayerName, ".") != 0) {
		PlayerName = vb6::Replace(PlayerName, ".", "");
	}
	Temps = GetVar(GetCharPath(PlayerName), "GUILD", "GUILDINDEX");
	if (vb6::IsNumeric(Temps)) {
		retval = vb6::CInt(Temps);
	} else {
		retval = 0;
	}
	return retval;
}

int GetGuildIndex(std::string GuildName) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'me da el indice del guildname */
	int i;

	// FIXME: Busqueda lineal!

	retval = 0;
	GuildName = vb6::UCase(GuildName);
	for (i = (1); i <= (CANTIDADDECLANES); i++) {
		if (vb6::UCase(guilds[i]->GuildName()) == GuildName) {
			retval = i;
			return retval;
		}
	}
	return retval;
}

std::string m_ListaDeMiembrosOnline(int UserIndex, int GuildIndex) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: 28/05/2010 */
	/* '28/05/2010: ZaMa - Solo dioses pueden ver otros dioses online. */
	/* '*************************************************** */

	bool esgm = false;

	/* ' Solo dioses pueden ver otros dioses online */
	if (UserTieneAlgunPrivilegios(UserIndex, PlayerType_Dios, PlayerType_Admin)) {
		esgm = true;
	}

	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES) {
		for (auto i : guilds[GuildIndex]->m_Iterador_ProximoUserIndex()) {
			/* 'No mostramos dioses y admins */
			if ((i != UserIndex)
					&& (UserTieneAlgunPrivilegios(i, PlayerType_User, PlayerType_Consejero,
							PlayerType_SemiDios) || esgm)) {
				retval = retval + UserList[i].Name + ",";
			}
		}
	}

	if (vb6::Len(retval) > 0) {
		retval = vb6::Left(retval, vb6::Len(retval) - 1);
	}
	return retval;
}

std::vector<std::string> PrepareGuildsList() {
	std::vector<std::string> retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	std::vector<std::string> tStr;
	int i;

	if (CANTIDADDECLANES == 0) {
		tStr.resize(0);
	} else {
		tStr.resize(CANTIDADDECLANES);

		for (i = (1); i <= (CANTIDADDECLANES); i++) {
			tStr[i - 1] = guilds[i]->GuildName();
		}
	}

	retval = tStr;
	return retval;
}

void SendGuildDetails(int UserIndex, std::string GuildName) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	std::vector<std::string> codex;
	int GI;
	int i;

	GI = GetGuildIndex(GuildName);
	if (GI == 0) {
		return;
	}

	codex.resize(CANTIDADMAXIMACODEX);

	for (i = (1); i <= (CANTIDADMAXIMACODEX); i++) {
		codex[i - 1] = guilds[GI]->GetCodex(i);
	}

	WriteGuildDetails(UserIndex, GuildName, guilds[GI]->Fundador(), guilds[GI]->GetFechaFundacion(),
			guilds[GI]->GetLeader(), guilds[GI]->GetURL(), guilds[GI]->CantidadDeMiembros(),
			guilds[GI]->EleccionesAbiertas(), Alineacion2String(guilds[GI]->Alineacion()),
			guilds[GI]->CantidadEnemys(), guilds[GI]->CantidadAllies(),
			vb6::CStr(guilds[GI]->PuntosAntifaccion()) + "/" + vb6::CStr(MAXANTIFACCION), codex,
			guilds[GI]->GetDesc());
}

void SendGuildLeaderInfo(int UserIndex) {
	/* '*************************************************** */
	/* 'Autor: Mariano Barrou (El Oso) */
	/* 'Last Modification: 12/10/06 */
	/* 'Las Modified By: Juan Martín Sotuyo Dodero (Maraxus) */
	/* '*************************************************** */
	int GI;
	std::vector<std::string> guildList;
	std::vector<std::string> MemberList;
	std::vector<std::string> aspirantsList;

	GI = UserList[UserIndex].GuildIndex;

	guildList = PrepareGuildsList();

	if (GI <= 0 || GI > CANTIDADDECLANES) {
		/* 'Send the guild list instead */
		WriteGuildList(UserIndex, guildList);
		return;
	}

	MemberList = guilds[GI]->GetMemberList();

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		/* 'Send the guild list instead */
		WriteGuildMemberInfo(UserIndex, guildList, MemberList);
		return;
	}

	aspirantsList = guilds[GI]->GetAspirantes();

	WriteGuildLeaderInfo(UserIndex, guildList, MemberList, guilds[GI]->GetGuildNews(), aspirantsList);
}

const std::set<int>& guild_Iterador_ProximoUserIndex(int GuildIndex) {
	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES)
		return guilds[GuildIndex]->m_Iterador_ProximoUserIndex();
	throw std::runtime_error("guild_Iterador_ProximoUserIndex");
}

//int m_Iterador_ProximoUserIndex(int GuildIndex) {
//	int retval;
//	/* '*************************************************** */
//	/* 'Author: Unknown */
//	/* 'Last Modification: - */
//	/* ' */
//	/* '*************************************************** */
//
//	/* 'itera sobre los onlinemembers */
//	retval = 0;
//	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES) {
//		retval = guilds[GuildIndex]->m_Iterador_ProximoUserIndex();
//	}
//	return retval;
//}
//

const std::set<int>& guild_Iterador_ProximoGM(int GuildIndex) {
	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES)
		return guilds[GuildIndex]->Iterador_ProximoGM();
	throw std::runtime_error("guild_Iterador_ProximoGM");
}

//int Iterador_ProximoGM(int GuildIndex) {
//	int retval;
//	/* '*************************************************** */
//	/* 'Author: Unknown */
//	/* 'Last Modification: - */
//	/* ' */
//	/* '*************************************************** */
//
//	/* 'itera sobre los gms escuchando este clan */
//	retval = 0;
//	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES) {
//		retval = guilds[GuildIndex]->Iterador_ProximoGM();
//	}
//	return retval;
//}
//
//int r_Iterador_ProximaPropuesta(int GuildIndex, RELACIONES_GUILD Tipo) {
//	int retval;
//	/* '*************************************************** */
//	/* 'Author: Unknown */
//	/* 'Last Modification: - */
//	/* ' */
//	/* '*************************************************** */
//
//	/* 'itera sobre las propuestas */
//	retval = 0;
//	if (GuildIndex > 0 && GuildIndex <= CANTIDADDECLANES) {
//		retval = guilds[GuildIndex]->Iterador_ProximaPropuesta(Tipo);
//	}
//	return retval;
//}

int GMEscuchaClan(int UserIndex, std::string GuildName) {
	int retval = 0;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;

	/* 'listen to no guild at all */
	if (vb6::LenB(GuildName) == 0 && UserList[UserIndex].EscucheClan != 0) {
		/* 'Quit listening to previous guild!! */
		WriteConsoleMsg(UserIndex,
				"You stop listening : " + guilds[UserList[UserIndex].EscucheClan]->GuildName(),
				FontTypeNames_FONTTYPE_GUILD);
		guilds[UserList[UserIndex].EscucheClan]->DesconectarGM(UserIndex);
		return retval;
	}

	/* 'devuelve el guildindex */
	GI = GetGuildIndex(GuildName);
	if (GI > 0) {
		if (UserList[UserIndex].EscucheClan != 0) {
			if (UserList[UserIndex].EscucheClan == GI) {
				/* 'Already listening to them... */
				WriteConsoleMsg(UserIndex, "Connected to : " + GuildName, FontTypeNames_FONTTYPE_GUILD);
				retval = GI;
				return retval;
			} else {
				/* 'Quit listening to previous guild!! */
				WriteConsoleMsg(UserIndex,
						"You stop listening : " + guilds[UserList[UserIndex].EscucheClan]->GuildName(),
						FontTypeNames_FONTTYPE_GUILD);
				guilds[UserList[UserIndex].EscucheClan]->DesconectarGM(UserIndex);
			}
		}

		guilds[GI]->ConectarGM(UserIndex);
		WriteConsoleMsg(UserIndex, "Connected to : " + GuildName, FontTypeNames_FONTTYPE_GUILD);
		retval = GI;
		UserList[UserIndex].EscucheClan = GI;
	} else {
		WriteConsoleMsg(UserIndex, "Error, the clan does not exist.", FontTypeNames_FONTTYPE_GUILD);
		retval = 0;
	}

	return retval;
}

void GMDejaDeEscucharClan(int UserIndex, int GuildIndex) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'el index lo tengo que tener de cuando me puse a escuchar */
	UserList[UserIndex].EscucheClan = 0;
	guilds[GuildIndex]->DesconectarGM(UserIndex);
}
int r_DeclararGuerra(int UserIndex, std::string & GuildGuerra, std::string & refError) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int GIG;

	retval = 0;
	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	if (vb6::Trim(GuildGuerra) == "") {
		refError = "You haven't chosen a valid clan.";
		return retval;
	}

	GIG = GetGuildIndex(GuildGuerra);
	if (guilds[GI]->GetRelacion(GIG) == RELACIONES_GUILD_GUERRA) {
		refError = "Your clan is already at war with " + GuildGuerra + ".";
		return retval;
	}

	if (GI == GIG) {
		refError = "You can't declare war to your own clan.";
		return retval;
	}

	if (GIG < 1 || GIG > CANTIDADDECLANES) {
		LogError("ModGuilds.r_DeclararGuerra: " + vb6::CStr(GI) + " declara a " + GuildGuerra);
		refError = "Clans system fatal error. Contact an admin (GIG out of range)";
		return retval;
	}

	guilds[GI]->AnularPropuestas(GIG);
	guilds[GIG]->AnularPropuestas(GI);
	guilds[GI]->SetRelacion(GIG, RELACIONES_GUILD_GUERRA);
	guilds[GIG]->SetRelacion(GI, RELACIONES_GUILD_GUERRA);

	retval = GIG;

	return retval;
}

int r_AceptarPropuestaDePaz(int UserIndex, std::string & GuildPaz, std::string & refError) {
	int retval = 0;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'el clan de userindex acepta la propuesta de paz de guildpaz, con quien esta en guerra */
	int GI;
	int GIG;

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	if (vb6::Trim(GuildPaz) == "") {
		refError = "You haven't chosen a valid clan.";
		return retval;
	}

	GIG = GetGuildIndex(GuildPaz);

	if (GIG < 1 || GIG > CANTIDADDECLANES) {
		LogError("ModGuilds.r_AceptarPropuestaDePaz: " + vb6::CStr(GI) + " acepta de " + GuildPaz);
		refError = "Clans system fatal error. Contact an admin (GIG out of range)";
		return retval;
	}

	if (guilds[GI]->GetRelacion(GIG) != RELACIONES_GUILD_GUERRA) {
		refError = "You are not at war with that clan.";
		return retval;
	}

	if (!guilds[GI]->HayPropuesta(GIG, RELACIONES_GUILD_PAZ)) {
		refError = "There are no peace proposals to accept.";
		return retval;
	}

	guilds[GI]->AnularPropuestas(GIG);
	guilds[GIG]->AnularPropuestas(GI);
	guilds[GI]->SetRelacion(GIG, RELACIONES_GUILD_PAZ);
	guilds[GIG]->SetRelacion(GI, RELACIONES_GUILD_PAZ);

	retval = GIG;
	return retval;
}

int r_RechazarPropuestaDeAlianza(int UserIndex, std::string & GuildPro, std::string & refError) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'devuelve el index al clan guildPro */
	int GI;
	int GIG;

	retval = 0;
	GI = UserList[UserIndex].GuildIndex;

	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	if (vb6::Trim(GuildPro) == "") {
		refError = "You haven't chosen a valid clan.";
		return retval;
	}

	GIG = GetGuildIndex(GuildPro);

	if (GIG < 1 || GIG > CANTIDADDECLANES) {
		LogError("ModGuilds.r_RechazarPropuestaDeAlianza: " + vb6::CStr(GI) + " acepta de " + GuildPro);
		refError = "Clans system fatal error. Contact an admin (GIG out of range).";
		return retval;
	}

	if (!guilds[GI]->HayPropuesta(GIG, RELACIONES_GUILD_ALIADOS)) {
		refError = "There's no alliance proposal with clan " + GuildPro;
		return retval;
	}

	guilds[GI]->AnularPropuestas(GIG);
	/* 'avisamos al otro clan */
	guilds[GIG]->SetGuildNews(
			guilds[GI]->GuildName() + " has rejected your alliance proposal. "
					+ guilds[GIG]->GetGuildNews());
	retval = GIG;

	return retval;
}

int r_RechazarPropuestaDePaz(int UserIndex, std::string & GuildPro, std::string & refError) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'devuelve el index al clan guildPro */
	int GI;
	int GIG;

	retval = 0;
	GI = UserList[UserIndex].GuildIndex;

	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	if (vb6::Trim(GuildPro) == "") {
		refError = "You haven't chosen a valid clan.";
		return retval;
	}

	GIG = GetGuildIndex(GuildPro);

	if (GIG < 1 || GIG > CANTIDADDECLANES) {
		LogError("ModGuilds.r_RechazarPropuestaDePaz: " + vb6::CStr(GI) + " acepta de " + GuildPro);
		refError = "Clans system fatal error. Contact and admin (GIG out of range).";
		return retval;
	}

	if (!guilds[GI]->HayPropuesta(GIG, RELACIONES_GUILD_PAZ)) {
		refError = "There's no peace proposal from " + GuildPro;
		return retval;
	}

	guilds[GI]->AnularPropuestas(GIG);
	/* 'avisamos al otro clan */
	guilds[GIG]->SetGuildNews(
			guilds[GI]->GuildName() + " has rejected our peace proposal. "
					+ guilds[GIG]->GetGuildNews());
	retval = GIG;

	return retval;
}

int r_AceptarPropuestaDeAlianza(int UserIndex, std::string & GuildAllie, std::string & refError) {
	int retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	/* 'el clan de userindex acepta la propuesta de paz de guildpaz, con quien esta en guerra */
	int GI;
	int GIG;

	retval = 0;
	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	if (vb6::Trim(GuildAllie) == "") {
		refError = "You haven't chosen a valid clan.";
		return retval;
	}

	GIG = GetGuildIndex(GuildAllie);

	if (GIG < 1 || GIG > CANTIDADDECLANES) {
		LogError("ModGuilds.r_AceptarPropuestaDeAlianza: " + vb6::CStr(GI) + " acepta de " + GuildAllie);
		refError = "Clans system fatal error. Contact and admin (GIG out of range).";
		return retval;
	}

	if (guilds[GI]->GetRelacion(GIG) != RELACIONES_GUILD_PAZ) {
		refError =
				"You are not at peace with that clan. You can only accept alliance proposals with clans you are at peace with.";
		return retval;
	}

	if (!guilds[GI]->HayPropuesta(GIG, RELACIONES_GUILD_ALIADOS)) {
		refError = "There are no alliance proposals to review.";
		return retval;
	}

	guilds[GI]->AnularPropuestas(GIG);
	guilds[GIG]->AnularPropuestas(GI);
	guilds[GI]->SetRelacion(GIG, RELACIONES_GUILD_ALIADOS);
	guilds[GIG]->SetRelacion(GI, RELACIONES_GUILD_ALIADOS);

	retval = GIG;

	return retval;
}

bool r_ClanGeneraPropuesta(int UserIndex, std::string & OtroClan, RELACIONES_GUILD Tipo,
		std::string & Detalle, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int OtroClanGI;
	int GI;

	retval = false;

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	OtroClanGI = GetGuildIndex(OtroClan);

	if (OtroClanGI == GI) {
		refError = "You can't declare an alliance with your own clan.";
		return retval;
	}

	if (OtroClanGI <= 0 || OtroClanGI > CANTIDADDECLANES) {
		refError = "Clans system fatal error. The other clan does not exist. Contact and admin.";
		return retval;
	}

	if (guilds[OtroClanGI]->HayPropuesta(GI, Tipo)) {
		refError = "A " + Relacion2String(Tipo) + " proposal with " + OtroClan + " already exists.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	/* 'de acuerdo al tipo procedemos validando las transiciones */
	if (Tipo == RELACIONES_GUILD_PAZ) {
		if (guilds[GI]->GetRelacion(OtroClanGI) != RELACIONES_GUILD_GUERRA) {
			refError = "You are not at war with " + OtroClan;
			return retval;
		}
	} else if (Tipo == RELACIONES_GUILD_GUERRA) {
		/* 'por ahora no hay propuestas de guerra */
	} else if (Tipo == RELACIONES_GUILD_ALIADOS) {
		if (guilds[GI]->GetRelacion(OtroClanGI) != RELACIONES_GUILD_PAZ) {
			refError = "To request an alliance you must not be either allied or at war with " + OtroClan;
			return retval;
		}
	}

	guilds[OtroClanGI]->SetPropuesta(Tipo, GI, Detalle);
	retval = true;

	return retval;
}

std::string r_VerPropuesta(int UserIndex, std::string & OtroGuild, RELACIONES_GUILD Tipo,
		std::string & refError) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int OtroClanGI;
	int GI;

	retval = "";
	refError = "";

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	OtroClanGI = GetGuildIndex(OtroGuild);

	if (!guilds[GI]->HayPropuesta(OtroClanGI, Tipo)) {
		refError = "The proposal does not exist.";
		return retval;
	}

	retval = guilds[GI]->GetPropuesta(OtroClanGI, Tipo);

	return retval;
}

std::vector<std::string> r_ListaDePropuestas(int UserIndex, RELACIONES_GUILD Tipo) {
	std::vector<std::string> retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int proposalCount;
	std::vector<std::string> proposals;

	GI = UserList[UserIndex].GuildIndex;

	if (GI > 0 && GI <= CANTIDADDECLANES) {
		proposalCount = guilds[GI]->CantidadPropuestas(Tipo);

		/* 'Resize array to contain all proposals */
		proposals.resize(proposalCount);

		/* 'Store each guild name */
		//for (i = (0); i <= (proposalCount - 1); i++) {
		int i = 0;
		for (auto e : guilds[GI]->Iterador_ProximaPropuesta(Tipo)) {
			proposals[i] = guilds[e]->GuildName();
			i++;
		}
	}

	retval = proposals;
	return retval;
}

void a_RechazarAspiranteChar(std::string & Aspirante, int guild, std::string & Detalles) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (vb6::InStrB(Aspirante, "/") != 0) {
		Aspirante = vb6::Replace(Aspirante, "/", "");
	}
	if (vb6::InStrB(Aspirante, "/") != 0) {
		Aspirante = vb6::Replace(Aspirante, "/", "");
	}
	if (vb6::InStrB(Aspirante, ".") != 0) {
		Aspirante = vb6::Replace(Aspirante, ".", "");
	}
	guilds[guild]->InformarRechazoEnChar(Aspirante, Detalles);
}

std::string a_ObtenerRechazoDeChar(std::string & Aspirante) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (vb6::InStrB(Aspirante, "/") != 0) {
		Aspirante = vb6::Replace(Aspirante, "/", "");
	}
	if (vb6::InStrB(Aspirante, "/") != 0) {
		Aspirante = vb6::Replace(Aspirante, "/", "");
	}
	if (vb6::InStrB(Aspirante, ".") != 0) {
		Aspirante = vb6::Replace(Aspirante, ".", "");
	}
	retval = GetVar(GetCharPath(Aspirante), "GUILD", "MotivoRechazo");
	WriteVar(GetCharPath(Aspirante), "GUILD", "MotivoRechazo", "");
	return retval;
}

bool a_RechazarAspirante(int UserIndex, std::string & Nombre, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int NroAspirante;

	retval = false;
	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	NroAspirante = guilds[GI]->NumeroDeAspirante(Nombre);

	if (NroAspirante == 0) {
		refError = Nombre + " has not requested to join your clan.";
		return retval;
	}

	guilds[GI]->RetirarAspirante(Nombre, NroAspirante);
	refError = "Your application to " + guilds[GI]->GuildName() + " has been rejected.";
	retval = true;

	return retval;
}

std::string a_DetallesAspirante(int UserIndex, std::string & Nombre) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int NroAspirante;

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		return retval;
	}

	NroAspirante = guilds[GI]->NumeroDeAspirante(Nombre);
	if (NroAspirante > 0) {
		retval = guilds[GI]->DetallesSolicitudAspirante(NroAspirante);
	}

	return retval;
}

void SendDetallesPersonaje(int UserIndex, std::string Personaje) {
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int NroAsp;
	std::string GuildName;
	std::shared_ptr<clsIniManager> UserFile;
	std::string Miembro;
	int GuildActual;
	std::vector<std::string> list;

	try {
		GI = UserList[UserIndex].GuildIndex;

		Personaje = vb6::UCase(Personaje);

		if (GI <= 0 || GI > CANTIDADDECLANES) {
			WriteConsoleMsg(UserIndex, "You are not part of a clan.",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
			WriteConsoleMsg(UserIndex, "You are not the leader of your clan.",
					FontTypeNames_FONTTYPE_INFO);
			return;
		}

		if (vb6::InStrB(Personaje, "/") != 0) {
			Personaje = vb6::Replace(Personaje, "/", "");
		}
		if (vb6::InStrB(Personaje, "/") != 0) {
			Personaje = vb6::Replace(Personaje, "/", "");
		}
		if (vb6::InStrB(Personaje, ".") != 0) {
			Personaje = vb6::Replace(Personaje, ".", "");
		}

		NroAsp = guilds[GI]->NumeroDeAspirante(Personaje);

		if (NroAsp == 0) {
			list = guilds[GI]->GetMemberList();

			if (std::find(list.begin(), list.end(), Personaje) == list.end()) {
				WriteConsoleMsg(UserIndex,
						"The PC is neither a postulant nor a member of the clan.",
						FontTypeNames_FONTTYPE_INFO);
				return;
			}
		}

		/* 'ahora traemos la info */

		UserFile.reset(new clsIniManager());

		UserFile->Initialize(GetCharPath(Personaje));

		/* ' Get the character's current guild */
		GuildActual = vb6::val(UserFile->GetValue("GUILD", "GuildIndex"));
		if (GuildActual > 0 && GuildActual <= CANTIDADDECLANES) {
			GuildName = "<" + guilds[GuildActual]->GuildName() + ">";
		} else {
			GuildName = "None";
		}

		/* 'Get previous guilds */
		Miembro = UserFile->GetValue("GUILD", "Miembro");
		if (vb6::Len(Miembro) > 400) {
			Miembro = ".." + vb6::Right(Miembro, 400);
		}

		WriteCharacterInfo(UserIndex, Personaje,
				static_cast<eRaza>(vb6::CInt(UserFile->GetValue("INIT", "Raza"))),
				static_cast<eClass>(vb6::CInt(
						UserFile->GetValue("INIT", "Clase"))),
				static_cast<eGenero>(vb6::CInt(
						UserFile->GetValue("INIT", "Genero"))),
				vb6::CInt(UserFile->GetValue("STATS", "ELV")),
				vb6::CInt(UserFile->GetValue("STATS", "GLD")),
				vb6::CInt(UserFile->GetValue("STATS", "Banco")),
				vb6::CInt(UserFile->GetValue("REP", "Promedio")),
				UserFile->GetValue("GUILD", "Pedidos"), GuildName, Miembro,
				vb6::CBool(
						vb6::CInt(
								UserFile->GetValue("FACCIONES",
										"EjercitoReal"))),
				vb6::CBool(
						vb6::CInt(
								UserFile->GetValue("FACCIONES",
										"EjercitoCaos"))),
				vb6::CInt(UserFile->GetValue("FACCIONES", "CiudMatados")),
				vb6::CInt(UserFile->GetValue("FACCIONES", "CrimMatados")));

		UserFile.reset();
	}
	catch (std::runtime_error &ex) {
		std::cerr << "runtime_error: " << ex.what() << std::endl;
		LogError(ex.what());
		if (!(FileExist(GetCharPath(Personaje), 0))) {
			LogError(
					"El usuario " + UserList[UserIndex].Name + " ("
							+ vb6::CStr(UserIndex)
							+ " ) ha pedido los detalles del personaje "
							+ Personaje + " que no se encuentra.");

			/* ' Fuerzo el borrado de la lista, lo deberia hacer el programa que borra pjs.. */
			guilds[GI]->RemoveMemberName(Personaje);
		}
	} catch (...) {
		LogError(
				vb6::CStr("[ + Err.Number + ]  + Err.description ")
						+ " En la rutina SendDetallesPersonaje, por el usuario "
						+ UserList[UserIndex].Name + " (" + vb6::CStr(UserIndex)
						+ " ), pidiendo información sobre el personaje "
						+ Personaje);
	}
	return;
}

bool a_NuevoAspirante(int UserIndex, std::string & clan, std::string & Solicitud, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	std::string ViejoSolicitado;
	int ViejoGuildINdex;
	int ViejoNroAspirante;
	int NuevoGuildIndex;

	retval = false;

	if (UserList[UserIndex].GuildIndex > 0) {
		refError = "You are already part of a clan, leave it before trying to join another one.";
		return retval;
	}

	if (EsNewbie(UserIndex)) {
		refError = "Newbies can't join clans.";
		return retval;
	}

	NuevoGuildIndex = GetGuildIndex(clan);
	if (NuevoGuildIndex == 0) {
		refError = "That clan doesn't exist. Contact an admin.";
		return retval;
	}

	if (!m_EstadoPermiteEntrar(UserIndex, NuevoGuildIndex)) {
		refError = "You can't enter a clan with "
				+ Alineacion2String(guilds[NuevoGuildIndex]->Alineacion()) + " alignment.";
		return retval;
	}

	if (guilds[NuevoGuildIndex]->CantidadAspirantes() >= MAXASPIRANTES) {
		refError =
				"The clan has too many applications. Contact a clan member so he can process that backlog.";
		return retval;
	}

	ViejoSolicitado = GetVar(GetCharPath(UserList[UserIndex].Name), "GUILD", "ASPIRANTEA");

	if (vb6::LenB(ViejoSolicitado) != 0) {
		/* 'borramos la vieja solicitud */
		ViejoGuildINdex = vb6::CInt(ViejoSolicitado);
		if (ViejoGuildINdex != 0) {
			ViejoNroAspirante = guilds[ViejoGuildINdex]->NumeroDeAspirante(UserList[UserIndex].Name);
			if (ViejoNroAspirante > 0) {
				guilds[ViejoGuildINdex]->RetirarAspirante(UserList[UserIndex].Name, ViejoNroAspirante);
			}
		} else {
			/* 'RefError = "Inconsistencia en los clanes, avise a un administrador" */
			/* 'Exit Function */
		}
	}

	guilds[NuevoGuildIndex]->NuevoAspirante(UserList[UserIndex].Name, Solicitud);
	retval = true;
	return retval;
}

bool a_AceptarAspirante(int UserIndex, std::string & Aspirante, std::string & refError) {
	bool retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	int GI;
	int NroAspirante;
	int AspiranteUI;

	/* 'un pj ingresa al clan :D */

	retval = false;

	GI = UserList[UserIndex].GuildIndex;
	if (GI <= 0 || GI > CANTIDADDECLANES) {
		refError = "You are not part of a clan.";
		return retval;
	}

	if (!m_EsGuildLeader(UserList[UserIndex].Name, GI)) {
		refError = "You are not the leader of your clan.";
		return retval;
	}

	NroAspirante = guilds[GI]->NumeroDeAspirante(Aspirante);

	if (NroAspirante == 0) {
		refError = "The PC hasn't applied.";
		return retval;
	}

	AspiranteUI = NameIndex(Aspirante);
	if (AspiranteUI > 0) {
		/* 'pj Online */
		if (!m_EstadoPermiteEntrar(AspiranteUI, GI)) {
			refError = Aspirante + " can't enter a clan with "
					+ Alineacion2String(guilds[GI]->Alineacion()) + " alignment.";
			guilds[GI]->RetirarAspirante(Aspirante, NroAspirante);
			return retval;
		} else if (UserList[AspiranteUI].GuildIndex != 0) {
			refError = Aspirante + " is already part of a clan.";
			guilds[GI]->RetirarAspirante(Aspirante, NroAspirante);
			return retval;
		}
	} else {
		if (!m_EstadoPermiteEntrarChar(Aspirante, GI)) {
			refError = Aspirante + " can't enter a clan with "
					+ Alineacion2String(guilds[GI]->Alineacion()) + " alignment.";
			guilds[GI]->RetirarAspirante(Aspirante, NroAspirante);
			return retval;
		} else if (GetGuildIndexFromChar(Aspirante)) {
			refError = Aspirante + " is already part of a clan.";
			guilds[GI]->RetirarAspirante(Aspirante, NroAspirante);
			return retval;
		}
	}
	/* 'el pj es aspirante al clan y puede entrar */

	guilds[GI]->RetirarAspirante(Aspirante, NroAspirante);
	guilds[GI]->AceptarNuevoMiembro(Aspirante);

	/* ' If player is online, update tag */
	if (AspiranteUI > 0) {
		RefreshCharStatus(AspiranteUI);
	}

	retval = true;
	return retval;
}

std::string GuildName(int GuildIndex) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (GuildIndex <= 0 || GuildIndex > CANTIDADDECLANES) {
		return retval;
	}

	retval = guilds[GuildIndex]->GuildName();
	return retval;
}

std::string GuildLeader(int GuildIndex) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (GuildIndex <= 0 || GuildIndex > CANTIDADDECLANES) {
		return retval;
	}

	retval = guilds[GuildIndex]->GetLeader();
	return retval;
}

std::string GuildAlignment(int GuildIndex) {
	std::string retval;
	/* '*************************************************** */
	/* 'Author: Unknown */
	/* 'Last Modification: - */
	/* ' */
	/* '*************************************************** */

	if (GuildIndex <= 0 || GuildIndex > CANTIDADDECLANES) {
		return retval;
	}

	retval = Alineacion2String(guilds[GuildIndex]->Alineacion());
	return retval;
}

std::string GuildFounder(int GuildIndex) {
	std::string retval;
	/* '*************************************************** */
	/* 'Autor: ZaMa */
	/* 'Returns the guild founder's name */
	/* 'Last Modification: 25/03/2009 */
	/* '*************************************************** */
	if (GuildIndex <= 0 || GuildIndex > CANTIDADDECLANES) {
		return retval;
	}

	retval = guilds[GuildIndex]->Fundador();
	return retval;
}

void SetNewGuildName(int GuildIndex, std::string & newGuildName) {
	/* '*************************************************** */
	/* 'Author: Lex! */
	/* 'Last Modification: 15/05/2012 */
	/* 'Va a la clase de guilds para setear GuildName nuevo */
	/* '*************************************************** */

	guilds[GuildIndex]->SetGuildName(newGuildName);
}
