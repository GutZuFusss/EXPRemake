/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
//#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	mem_zero(&m_aNumTele, sizeof(m_aNumTele));
	mem_zero(&m_apTeleports, sizeof(m_apTeleports));

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int TeleIndex = m_pTiles[i].m_Index - TELEPORT_OFFSET;
		if(TeleIndex >= 0 && TeleIndex < NUM_TELEPORTS)
		{
			if(TeleIndex&1) //from
				m_aNumTele[TeleIndex]++;
		}
	}
	
	for(int i = 0; i < NUM_TELEPORTS; i++)
	{
		if(m_aNumTele[i] > 0)
			m_apTeleports[i] = new int[m_aNumTele[i]];
	}
	
	int CurTele[NUM_TELEPORTS] = {0};
	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int TeleIndex = m_pTiles[i].m_Index - TELEPORT_OFFSET;
		if(TeleIndex >= 0 && TeleIndex < NUM_TELEPORTS)
		{
			if(TeleIndex&1) //from
				m_apTeleports[TeleIndex][CurTele[TeleIndex]++] = i;
		}
	}

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;

		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK;
			break;
		case TILE_HEALING:
			m_pTiles[i].m_Index = COLFLAG_HEALING;
			break;
		case TILE_POISON:
			m_pTiles[i].m_Index = COLFLAG_POISON;
			break;
		case TILE_WEAPONSTRIP:
			m_pTiles[i].m_Index = COFLAG_WEAPONSTRIP;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}

		int TeleIndex = Index - TELEPORT_OFFSET;
		if(TeleIndex >= 0 && TeleIndex < NUM_TELEPORTS)
			m_pTiles[i].m_Index = 128+TeleIndex;
	}
}

int CCollision::GetTile(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
}

bool CCollision::IsTileSolid(int x, int y)
{
	return GetTile(x, y)&COLFLAG_SOLID;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, bool CheckDoors)
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i < End; i++)
	{
		float a = i/Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y) || (CheckDoors && IsDoor(Pos.x, Pos.y)))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

vec2 CCollision::Teleport(int x, int y)
{
	int nx = clamp(x/32, 0, m_Width-1);
	int ny = clamp(y/32, 0, m_Height-1);
	
	int TeleIndex = m_pTiles[ny*m_Width+nx].m_Index - 128;
	if(TeleIndex < 0 || TeleIndex >= NUM_TELEPORTS)
		return vec2(-1, -1);
	
	if(TeleIndex&1) //from
		return vec2(-1, -1);
	
	if(m_aNumTele[TeleIndex+1] == 0)
		return vec2(-1, -1);
	
	vec2 Closest = vec2(-1, -1);
	float ClosestDistance = 0.0f;
	for(int i = 0; i < m_aNumTele[TeleIndex+1]; i++)
	{
		int Dest = m_apTeleports[TeleIndex+1][i];
		vec2 DestPos = vec2((Dest%m_Width)*32+16, (Dest/m_Width)*32+16);
		float d = distance(vec2(x, y), DestPos);
		if(Closest == vec2(-1, -1) || d < ClosestDistance)
		{
			ClosestDistance = d;
			Closest = DestPos;
		}
	}
	return Closest;
}

void CCollision::SetDoor(int StartX, int StartY, int EndX, int EndY)
{
	int NStartX = clamp(StartX/32, 0, m_Width-1);
	int NStartY = clamp(StartY/32, 0, m_Height-1);
	int NEndX = clamp(EndX/32, 0, m_Width-1);
	int NEndY = clamp(EndY/32, 0, m_Height-1);
	
	for(int nx = NStartX; nx <= NEndX; nx++)
	{
		for(int ny = NStartY; ny <= NEndY; ny++)
		{
			m_pTiles[ny*m_Width+nx].m_Index |= COLFLAG_NOHOOK|COLFLAG_DOOR;
		}
	}
}

int CCollision::IsDoor(int x, int y)
{
	return GetCollisionAt(x, y)&COLFLAG_DOOR;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces)
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size)
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity)
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}
