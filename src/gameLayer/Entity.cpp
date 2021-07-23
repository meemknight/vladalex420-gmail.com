#include "Entity.h"
#include "glm/glm.hpp"
#include <algorithm>
#include "gameMath.h"
#include <time.h>
#include <cstdlib>
#include "Particle.h"
#include "input.h"

float gravitationalAcceleration = 64;
float gravitationalAccelerationOnIce = 5;
float jumpSpeed = 20.5;
float jumpFromWallSpeed = 22;
float velocityClampY = 30;
float velocityClampX = 10;
float drag = 0.15f;
float dragX = 0.20f;
float strafeSpeed = 8;
float strafeSpeedMove = 10;
float runSpeed = 14;
float airRunSpeed = 10;
float grabMargin = 0.55f;
float notGrabTimeVal = 0.06;
bool snapWallGrab = 0;
float ghostJumpTime = 0.08;
float iceSlipX = 3;
float velocityOnAirRun = 10.f;

float arrowSpeed = 20;

#undef max;
#undef min;

extern Entity player;
extern std::vector<Particle>crackParticles;
extern std::vector <LightSource> wallLights;

//pos and size
bool aabb(glm::vec4 b1, glm::vec4 b2)
{
	if (((b1.x - b2.x < b2.z)
		&& b2.x - b1.x < b1.z
		)
		&& ((b1.y - b2.y < b2.w)
			&& b2.y - b1.y < b1.w
			)
		)
	{
		return 1;
	}
	return 0;
}


void Entity::checkCollisionBrute(glm::vec2 &pos, glm::vec2 lastPos, MapData & mapData,
	bool &upTouch, bool &downTouch, bool &leftTouch, bool &rightTouch)
{

	glm::vec2 delta = pos - lastPos;

	if (
		(pos.y < -dimensions.y)
		|| (pos.x < -dimensions.x)
		|| (pos.y > mapData.h * BLOCK_SIZE)
		|| (pos.x > mapData.w * BLOCK_SIZE)
		)
	{
		return;
	}

	
	glm::vec2 newPos = performCollision(mapData, { pos.x, lastPos.y }, { dimensions.x, dimensions.y }, { delta.x, 0 },
		upTouch, downTouch, leftTouch, rightTouch);
	pos = performCollision(mapData, { newPos.x, pos.y }, { dimensions.x, dimensions.y }, { 0, delta.y },
		upTouch, downTouch, leftTouch, rightTouch);

}

void Entity::resolveConstrains(MapData & mapData)
{
	
	bool upTouch = 0;
	bool downTouch = 0;
	bool leftTouch = 0;
	bool rightTouch = 0;
	
	float distance = glm::length(lastPos - pos);
	
	if (distance < BLOCK_SIZE)
	{
		checkCollisionBrute(pos,
			lastPos,
			mapData,
			upTouch,
			downTouch,
			leftTouch,
			rightTouch
			);
	}
	else
	{
		glm::vec2 newPos = lastPos;
		glm::vec2 delta = pos - lastPos;
		delta = glm::normalize(delta);
		delta *= 0.9 * BLOCK_SIZE;
	
		do
		{
			newPos += delta;
			glm::vec2 posTest = newPos;
			checkCollisionBrute(newPos,
				lastPos,
				mapData,
				upTouch,
				downTouch,
				leftTouch,
				rightTouch);
	
			if (newPos != posTest)
			{
				pos = newPos;
				goto end;
			}
	
		} while (glm::length((newPos + delta) - pos) > 1.0f * BLOCK_SIZE);
	
		checkCollisionBrute(pos,
			lastPos,
			mapData,
			upTouch,
			downTouch,
			leftTouch,
			rightTouch);
	}
	
	end:
	
	if (pos.x < 0) { pos.x = 0; }
	if (pos.x + dimensions.x > (mapData.w) * BLOCK_SIZE ) { pos.x = ((mapData.w) * BLOCK_SIZE )-dimensions.x; }

	//todo maby slow this down
	if (upTouch)
	{
		if (velocity.y < 0)
		{
			velocity.y *= 0;
		}
	}

}

float Entity::getAcceleration()
{
	float a = std::max((accelerateTime / maxAccelerationTime), 0.6f);
	if (a <= 0.9) { return a; }
	return a;
}

void Entity::updateMove(float deltaTime)
{
	if (lastPos.x - pos.x < 0)
	{
		movingRight = 1;
	}
	else if (lastPos.x - pos.x > 0)
	{
		movingRight = 0;
	}

	if (wallGrab == -1)
	{
		movingRight = 0;
	}
	if (wallGrab == 1)
	{
		movingRight = 1;
	}

	lastPos = pos;

	if (!accelerating)
	{
		if (accelerateTime>0)
		{
			accelerateTime -= deltaTime;
		}
		else
		{
			accelerateTime = 0;
		}
	}
	else
	{
		accelerateTime += deltaTime;
		if (accelerateTime > maxAccelerationTime)
		{
			accelerateTime = maxAccelerationTime;
		}
	}

	accelerating = 0;
}

void Entity::strafe(int dir)
{
	velocity.x = dir * strafeSpeed * BLOCK_SIZE;
}

constexpr float iceAcceleration = 10;

void Entity::run(float speed, float deltaTime)
{
	if (isExitingLevel != -1 || lockMovementDie)
	{
		return;
	}
	//if (player.notCrawTime > 0) { return; }

	if(iswebs)
	{
		pos.x += speed * runSpeed * BLOCK_SIZE * 0.1 * getAcceleration();
	}else
	{
		pos.x += speed * runSpeed * BLOCK_SIZE * getAcceleration();
	}
	moving = (bool)speed;

	if (speed) { accelerating = true; }

	if(isSittingOnIce && speed)
	{
		float f = speed < 0 ? -1 : 1;

		velocity.x += f * iceSlipX * BLOCK_SIZE * deltaTime * iceAcceleration;

		if(speed < 0)
		{
			velocity.x = std::max(-1 * iceSlipX * BLOCK_SIZE, velocity.x);
		}else
		{
			velocity.x = std::min(1 * iceSlipX * BLOCK_SIZE, velocity.x);
		}

	}

	//todo check what i did earlier
	//if(!isSittingOnIce && speed)
	//{
	//	velocity.x = 0;
	//}

}

void Entity::airRun(float speed)
{
	if(lockMovementDie)
	{
		return;
	}

	if(isExitingLevel!=-1)
	{
		return;
	}

	//if (player.notCrawTime > 0) { return; }

	if (dying)
	{
		speed *= 0.5;
	}

	if(speed)
	{
		accelerating = 1;
	}

	if(speed > 0)
	{
		if(velocity.x < -strafeSpeedMove * BLOCK_SIZE)
		{
			return;
		}
	}else 
	if (speed < 0)
	{
		if (velocity.x > strafeSpeedMove * BLOCK_SIZE)
		{
			return;
		}
	}
	
	float dir = speed < 0 ? -1 : 1;
	float velocityDir = velocity.x < 0 ? -1 : 1;


	//this removes velocity when grounded
	if((grounded && !isSittingOnIce) || ((dir != velocityDir) && speed)) 
	{ velocity.x = 0; }
	
	//this removes velocity when moving in air
	if (speed && !isSittingOnIce ) 
	{ velocity.x = dir * velocityOnAirRun; }

	if(iswebs)
	{
		pos.x += speed * airRunSpeed * BLOCK_SIZE * 0.1;
	}else
	{
		pos.x += speed * airRunSpeed * BLOCK_SIZE;
	}

}

void Entity::applyGravity(float deltaTime)
{
	if(wallGrab == 0)
	{
			velocity.y += deltaTime * gravitationalAcceleration * BLOCK_SIZE;		
	}else if(iceGrab)
	{
		velocity.y += deltaTime * gravitationalAccelerationOnIce * BLOCK_SIZE;
	}
}

void Entity::applyVelocity(float deltaTime)
{
	if(dying || lockMovementDie)
	{
		return;
	}

	if (notGrabTime <= 0) { notGrabTime = 0; }
	else
	{
		notGrabTime -= deltaTime;
	}

	const float cx = velocityClampX * BLOCK_SIZE;
	const float cy = velocityClampY * BLOCK_SIZE;
	velocity = glm::clamp(velocity, { -cx,-cy }, { cx, cy });

	if((wallGrab != 0 && !iceGrab) && (notCrawTime <= 0))
	{
		velocity.y = 0;
	}

	pos += velocity * deltaTime;

	//drag
	float newDrag = dragX;

	if(iswebs)
	{
		newDrag *= 2;
	}

	if(isSittingOnIce)
	{
		//newDrag /= 2.f;
	}

	velocity.x += velocity.x * (-newDrag * deltaTime * BLOCK_SIZE);


	if(iswebs)
	{
		velocity.y += velocity.y * (-drag * deltaTime * BLOCK_SIZE*4);
	}

	if(((grounded && !isSittingOnIce) || wallGrab ) && (notCrawTime <= 0))
	{
		velocity.x = 0;
	}


	if (std::fabs(velocity.x) < 0.1)
	{
		velocity.x = 0;
	}

	if (std::fabs(velocity.y) < 0.01)
	{
		velocity.y = 0;
	}

	if (grounded && velocity.y > 0)
	{
		velocity.y = 20;
	}

}

void Entity::checkGrounded(MapData &mapDat, float deltaTime)
{
	grounded = 0;
	canJump = 0;

	if (pos.y < -dimensions.y)
	{
		return;
	}

	int minx = floor((pos.x ) / BLOCK_SIZE);
	int maxx = floor((pos.x + dimensions.x - 1 ) / BLOCK_SIZE);

	minx = std::max(minx, 0);
	maxx = std::min(maxx, mapDat.w);

	isSittingOnIce = false;

	for (int x = minx; x <= maxx; x++)
	{
		if(isIce(mapDat.get(x, floor((pos.y + dimensions.y) / BLOCK_SIZE)).type))
		{
			isSittingOnIce = true;
		}
		
		auto b = mapDat.get(x, floor((pos.y + dimensions.y) / BLOCK_SIZE)).type;
		if (isCollidable(b) || 
			(isPlatofrm(b)
				&&
				player.lastPos.y + player.dimensions.y <= floor((pos.y + dimensions.y) / BLOCK_SIZE) * BLOCK_SIZE
				&& ! input::isKeyHeld(input::Buttons::down)
				)
			)
		{
			grounded = 1;
			canJump = 1;
			hasTouchGround = 1;
		}
	}

	if(!grounded && hasTouchGround == 1) //activate countdown
	{ 
		timeLeftGrounded = ghostJumpTime;
		canJump = 1;
		hasTouchGround = 0;
	}else if(timeLeftGrounded > 0)
	{
		timeLeftGrounded -= deltaTime;
		canJump = 1;
	}
	

}

void Entity::checkWall(MapData & mapData, int move)
{
	if (iswebs) 
	{ return; }

	if (pos.y < -BLOCK_SIZE)
	{
		return;
	}

	if(grounded)
	{
		return;
	}

	int minY = floor(((pos.y )/BLOCK_SIZE)+0.1f);
	float dist = (pos.y / BLOCK_SIZE) + 0.1f - floor((pos.y / BLOCK_SIZE) + 0.1f);
	
	if(minY <0)
	{
		return;
	}

	if(dist > grabMargin)
	{
		return;
	}
	
	bool checkRight = 0;
	bool checkLeft = 0;

	if (notGrabTime > 0)
	{
		if(lastWallGrab==-1)
		{
			checkRight = 1;
		}else if(lastWallGrab==1)
		{
			checkLeft = 1;
		}
	}else
	{
		checkRight = 1;
		checkLeft = 1;
	}

	int rightX = floor((pos.x + dimensions.x + 2) / BLOCK_SIZE);
	int leftX = floor((pos.x-2) / BLOCK_SIZE);
	if (leftX < 0) { return; }
	
	if(iceGrab)
	{
		if(wallGrab == 1)
		{
			move = 1;
			//checkRight = 1;
		}else if(wallGrab == -1)
		{
			move = -1;
			//checkLeft = 1;
		}
	
		wallGrab = 0;
		iceGrab = 0;
	}

	if(isCollidable(mapData.get(rightX, minY).type) && move > 0 && checkRight &&
		mapData.get(rightX, minY).type != Block::bareer&&
		!isCollidable(mapData.get(rightX-1, minY+1).type)
		)
	{

		if (isRedSolid(mapData.get(rightX, minY).type))
		{
			redGrab = 1;
		}
		if (isBlueSolid(mapData.get(rightX, minY).type))
		{
			blueGrab = 1;
		}
		if (mapData.get(rightX, minY).type == Block::fenceSolid)
		{
			grayGrab = 1;
		}

		if(isIce(mapData.get(rightX, minY).type))
		{
			iceGrab = 1;
		}

		//if ((minY == 0 || !isColidable(mapData.get(rightX, minY - 1).type)))
		if(wallGrab == 0)
		{
			if(snapWallGrab)
			{
				pos.y = minY * BLOCK_SIZE;
			}
			pos.x = (rightX - 1)*BLOCK_SIZE;
			wallGrab = 1;
			velocity.x = 0;

			if(!iceGrab)
			{
				velocity.y = 0;
			}
		}
	}


	if (isCollidable(mapData.get(leftX, minY).type) && move < 0 &&
		checkLeft && mapData.get(leftX, minY).type != Block::bareer &&
		!isCollidable(mapData.get(leftX + 1, minY + 1).type)
		)
	{

		if(isRedSolid(mapData.get(leftX, minY).type))
		{
			redGrab = 1;
		}
		if (isBlueSolid(mapData.get(leftX, minY).type))
		{
			blueGrab = 1;
		}
		if (mapData.get(leftX, minY).type == Block::fenceSolid)
		{
			grayGrab = 1;
		}

		if (isIce(mapData.get(leftX, minY).type))
		{
			iceGrab = 1;
		}

		//if (minY == 0 || !isColidable(mapData.get(leftX, minY - 1).type))
		if (wallGrab == 0)
		{
			if (snapWallGrab)
			{
				pos.y = minY * BLOCK_SIZE;
			}
			pos.x = (leftX + 1)*BLOCK_SIZE;
			wallGrab = -1;
			velocity.x = 0;
			
			if(!iceGrab)
			{
				velocity.y = 0;
			}

		}
	}

}

void Entity::jump()
{

	if(iswebs)
	{
		velocity.y = -jumpSpeed * BLOCK_SIZE / 2;
	}else
	{
		velocity.y = -jumpSpeed * BLOCK_SIZE;
	}

}

int randVal = 1;

void Entity::draw(gl2d::Renderer2D & renderer2d, float deltaTime, gl2d::Texture characterSprite)
{
	auto s = characterSprite.GetSize();
	gl2d::TextureAtlasPadding playerAtlas(4, 8, s.x,s.y);



	int state = 0;

	if (wallGrab != 0)
	{
		state = 3;
	}else
	if(!grounded)
	{
		state = 2;
	}else if(moving)
	{
		state = 1;
	}

	if(dying)
	{
		state = 4;
	}
	moving = 0;

	//idle animation
	if (state == 0)
	{
		idleTime += deltaTime;
		if (((int)idleTime/10)%2==1)
		{
			state = 5 + randVal;
		}else
		{
			randVal = rand() % 2;
		}
	}else
	{
		idleTime = 0;
	}


	currentCount += deltaTime;

	if (state == 0)
	{
		while (currentCount >= idlefFameDuration)
		{
			currentCount -= idlefFameDuration;
			currentFrame++;
		}
	}
	else
	{
		while (currentCount >= frameDuration)
		{
			currentCount -= frameDuration;
			currentFrame++;
		}
	}

	

	currentFrame %= 4;


	renderer2d.renderRectangle({ pos - glm::vec2(0,0),  8, 8 }, {}, 0, characterSprite,
		playerAtlas.get(currentFrame, state, !movingRight));

}

void Entity::jumpFromWall()
{
	notGrabTime = notGrabTimeVal;
	lastWallGrab = wallGrab;
	velocity.y = -jumpFromWallSpeed * BLOCK_SIZE;
}

glm::vec2 Entity::performCollision(MapData & mapData, glm::vec2 pos, glm::vec2 size, glm::vec2 delta,
	bool &upTouch, bool &downTouch, bool &leftTouch, bool &rightTouch)
{
	int minX=0;
	int minY=0;
	int maxX=mapData.w;
	int maxY=mapData.h;

	minX = (pos.x - abs(delta.x) - BLOCK_SIZE)/BLOCK_SIZE;
	maxX = ceil((pos.x + abs(delta.x) + BLOCK_SIZE + size.x)/BLOCK_SIZE);
	
	minY = (pos.y - abs(delta.y) - BLOCK_SIZE)/BLOCK_SIZE;
	maxY = ceil((pos.y + abs(delta.y) + BLOCK_SIZE + size.y)/BLOCK_SIZE);

	minX = std::max(0, minX);
	minY = std::max(0, minY);
	maxX = std::min(mapData.w, maxX);
	maxY = std::min(mapData.h, maxY);

	for (int y = minY; y < maxY; y++)
		for (int x = minX; x < maxX; x++)
		{
			if (isCollidable(mapData.get(x, y).type))
			{
				if(aabb({ pos,dimensions }, { x*BLOCK_SIZE, y*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }))
				{
					if (delta.x != 0) 
					{
						if(delta.x < 0) // moving left
						{
							leftTouch = 1;
							pos.x = x * BLOCK_SIZE + BLOCK_SIZE;
							goto end;
						}else
						{
							rightTouch = 1;
							pos.x = x * BLOCK_SIZE - dimensions.x;
							goto end;
						}
					}
					else
					{
						if(delta.y < 0) //moving up
						{
							upTouch = 1;
							pos.y = y * BLOCK_SIZE + BLOCK_SIZE;
							goto end;
						}else
						{
							downTouch = 1;
							pos.y = y * BLOCK_SIZE - dimensions.y;
							goto end;
						}
					}
				
				}
			}
			else if (isPlatofrm(mapData.get(x, y).type) && !input::isKeyHeld(input::Buttons::down))
			{
				if (aabb({ pos,dimensions }, { x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }))
				{
					if (delta.y != 0 && lastPos.y + dimensions.y <= y*BLOCK_SIZE)
					{
						downTouch = 1;
						pos.y = y * BLOCK_SIZE - dimensions.y;
						goto end;
					
					}

				}
			}

		}

	end:
	return pos;
}


#pragma region arrow

void Arrow::draw(gl2d::Renderer2D & renderer, gl2d::Texture t, gl2d::TextureAtlasPadding ta)
{

	float angle = 0;
	
	angle = std::asin(-shootDir.y);
	angle = glm::degrees(angle);
	
	if(shootDir.x < 0)
	{
		angle = 180.f - angle;
	}
	
	float dim = 1;
	if(liveTime < 1)
	{
		dim = liveTime;
		dim = std::max(dim, 0.f);
	}

	renderer.renderRectangle({ pos.x - BLOCK_SIZE, pos.y - (BLOCK_SIZE / 2.f),BLOCK_SIZE, BLOCK_SIZE }, { 1,1,1,light*dim }, { BLOCK_SIZE/2,0 }, angle, t, ta.get(type,0));
}

void Arrow::move(float deltaTime)
{
	if(!stuckInWall)
	{
		lastPos = pos;
		//toto arrow speed
		pos += shootDir * deltaTime * arrowSpeed;
		
	}
}

void Arrow::checkCollision(MapData &mapData, bool redTouch, bool blueTouch, bool grayTouch,
	int& redChanged, int& blueChanged, int& grayChanged, glm::ivec2 litTorches[], int litTorchesCount)
{
	if(stuckInWall)
	{
		return;
	}

	glm::vec2 curPos = lastPos;
	bool done = 0;

	float affinity = 0.02;

	while(!done)
	{
		if (glm::length(pos - curPos) > BLOCK_SIZE * affinity)
		{
			curPos.x += shootDir.x * BLOCK_SIZE * affinity;
			curPos.y += shootDir.y * BLOCK_SIZE * affinity;

		}else
		{
			curPos = pos;
			done = 1;
		}

		if (pos.x < 0
			|| pos.y < 0
			|| pos.x >(mapData.w)*BLOCK_SIZE
			|| pos.y >(mapData.h)*BLOCK_SIZE) {
			break;
		}
		auto &t = mapData.get(curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE).type;

		if (!stuckInWall)
		{
			if (t == Block::webBlock)
			{
				stuckInWall = 1;
				hitOnce = 1;
			}

			if (t == Block::targetRed)
			{
				stuckInWall = 1;
				if(!redTouch)
				{
					if(player.redGrab)
					{
						player.redGrab = 0;
						player.blueGrab = 0;
						player.grayGrab = 0;
						player.wallGrab = 0;
					}

					for (int i = 0; i < mapData.w * mapData.h; i++)
					{
						if (isRedSolid(mapData.data[i].type))
						{
							mapData.data[i].type++;
						}
						else if (isRedNoSolid(mapData.data[i].type))
						{
							mapData.data[i].type--;
						}
					}
					mapData.setNeighbors();
					redChanged = !redChanged;
				}
			

			}
			else if (t == Block::targetBlue)
			{
				stuckInWall = 1;
			
				if(!blueTouch)
				{
					if (player.blueGrab)
					{
						player.redGrab = 0;
						player.blueGrab = 0;
						player.grayGrab = 0;
						player.wallGrab = 0;
					}

					for (int i = 0; i < mapData.w * mapData.h; i++)
					{
						if (isBlueSolid(mapData.data[i].type))
						{
							mapData.data[i].type++;
						}
						else if (isBlueNoSolid(mapData.data[i].type))
						{
							mapData.data[i].type--;
						}
					}
					mapData.setNeighbors();
					blueChanged = !blueChanged;
				}
			
			}
			else if (t == Block::targetKey)
			{
				if (type == Arrow::ArrowTypes::keyArrow)
				{
					stuckInWall = 1;
					if(!grayTouch)
					{
						if (player.grayGrab)
						{
							player.redGrab = 0;
							player.blueGrab = 0;
							player.grayGrab = 0;
							player.wallGrab = 0;
						}

						for (int i = 0; i < mapData.w * mapData.h; i++)
						{
							if (mapData.data[i].type == Block::fenceSolid)
							{
								mapData.data[i].type++;
							}
							else if (mapData.data[i].type == Block::fenceNoSolid)
							{
								mapData.data[i].type--;
							}
						}
						mapData.setNeighbors();
						grayChanged = !grayChanged;
					}
					
				}
				

			}
			else if (t == Block::rockCracked)
			{
				if (type == Arrow::ArrowTypes::bombArrow)
				{
					auto &m = mapData.get(curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE);

					stuckInWall = 1;
					m.type = Block::none;
					mapData.setNeighbors();

					{//particle
						Particle p;
						p.maxDuration = 1.2;
						p.frameCount = 5;
						p.set({ (int)(curPos.x), (int)(curPos.y) }, 0, 1);
						crackParticles.push_back(p);
					}

					int minX = (curPos.x / BLOCK_SIZE) - 1;
					int maxX = (curPos.x / BLOCK_SIZE) + 1;
					int minY = (curPos.y / BLOCK_SIZE) - 1;
					int maxY = (curPos.y / BLOCK_SIZE) + 1;

					minX = std::max(0, minX);
					minY = std::max(0, minY);
					maxX = std::min(mapData.w, maxX);
					maxY = std::min(mapData.h, maxY);

					for(int y=minY; y<=maxY; y++)
						for (int x = minX; x <= maxX; x++)
						{
							if(mapData.get(x,y).type == Block::rockCracked)
							{
								mapData.get(x, y).type = Block::none;
								{//particle
									Particle p;
									p.maxDuration = 1.2;
									p.frameCount = 5;
									p.set({ x,y }, 0, 1);
									crackParticles.push_back(p);
								}

							}
						}

				}

			}
			else if (unLitTorch(t))
			{
				if (type == Arrow::ArrowTypes::fireArrow)
				{
					t++; //light the torch texture up
					int x = curPos.x / BLOCK_SIZE;
					int y = curPos.y / BLOCK_SIZE;
					auto it = std::find_if(wallLights.begin(), wallLights.end(), [x, y](LightSource &ls)
					{
						return ls.pos.x == x && ls.pos.y == y;
					});

					if (it != wallLights.end())
					{
						it->animationDuration = it->animationStartTime;
						it->intensity = mapData.getTorchLightIntensity(x, y);

						litTorches[litTorchesCount++] = glm::ivec2{ x,y };
					}
					//wallLights.push_back({ { curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE }, 0, 
					//	mapData.getTorchLight( curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE ) });
				}

			}
		}

		if(isCollidableForArrows(mapData.get(curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE).type))
		{
			auto t = mapData.get(curPos.x / BLOCK_SIZE, curPos.y / BLOCK_SIZE).type;

			
			if(type == Arrow::ArrowTypes::slimeArrow && hitOnce == 0)
			{
				hitOnce = 1;
			

				if ((lastPos.x < floor(curPos.x / BLOCK_SIZE)*BLOCK_SIZE && shootDir.x > 0) ||
					(lastPos.x > floor((curPos.x / BLOCK_SIZE) + 1)*BLOCK_SIZE&& shootDir.x < 0))
				{
					auto pos = glm::ivec2(lastPos.x, lastPos.y);
					pos.x /= BLOCK_SIZE;
					pos.y /= BLOCK_SIZE;
					if(shootDir.x<0 && pos.x>0 && isCollidableForArrows(mapData.get(pos.x-1, pos.y).type))
					{
						shootDir.x *= -1;
					}else
					if (pos.x < mapData.w - 1 && isCollidableForArrows(mapData.get(pos.x + 1, pos.y).type))
					{
						shootDir.x *= -1;
					}
					else
					{
						shootDir.y *= -1;
					}

				}else
				{
			
					auto pos = glm::ivec2(lastPos.x, lastPos.y);
					pos.x /= BLOCK_SIZE;
					pos.y /= BLOCK_SIZE;
					if (shootDir.y > 0 && pos.y < mapData.h-1 && isCollidableForArrows(mapData.get(pos.x, pos.y+1).type))
					{
						shootDir.y *= -1;
					}
					else
					if (pos.y > 0 && isCollidableForArrows(mapData.get(pos.x, pos.y-1).type))
					{
						shootDir.y *= -1;
					}
					else
					{
						shootDir.x *= -1;
					}

				}

				curPos.x += shootDir.x * BLOCK_SIZE * affinity*2;
				curPos.y += shootDir.y * BLOCK_SIZE * affinity*2;
				pos = curPos;
			}else
			{
				stuckInWall = 1;
			}

			break;
		}
	}

}

bool Arrow::leftMap(int w, int h)
{
	if(pos.x < -30*BLOCK_SIZE
		|| pos.y < -30 * BLOCK_SIZE
		||pos.x > (w+30)*BLOCK_SIZE
		|| pos.y >(h + 30)*BLOCK_SIZE) {
		return true;
	}

	return false;
}

bool Arrow::timeOut(float deltaTime)
{
	if(stuckInWall)
	{
		liveTime -= deltaTime;
		if (liveTime < 0) { return true; }
	}else
	{
		if(type == fireArrow)
		{
			liveTime = 15;
		}
	}

	return false;
}

#pragma endregion


#pragma region pickup

bool Pickup::colidePlayer(Entity &player)
{
	return aabb({ player.pos, player.dimensions }, { pos.x*BLOCK_SIZE, pos.y*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE });
}

void Pickup::draw(gl2d::Renderer2D & renderer2d, gl2d::Texture arrowTexture, 
	gl2d::TextureAtlasPadding ta, float deltaTime)
{

	float levitate = cos((animPos += deltaTime) / 0.7f);
	float opacity = 1;

	if(cullDown<=0)
	{
		cullDown = 0;
		opacity = 1;
	}else
	{
		cullDown -= deltaTime;
		opacity = 0.4;
	}
	
	levitate += 1;
	levitate /= 2.f;

	levitate *= 0.8;

	renderer2d.renderRectangle({ pos.x * BLOCK_SIZE,(pos.y - levitate) * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, 
		{ opacity, opacity, opacity, light * opacity }, {}, 45,
		arrowTexture, ta.get(type, 0));
}

#pragma endregion


#pragma region birb brother

float birdSpeed = BLOCK_SIZE * 7;

void Bird::update(float deltaTime)
{
	if(isMovingType != 0 )
	{
		texturePos.y = 2;

		if(glm::distance(position, destination) > birdSpeed * deltaTime)
		{
			if (glm::distance(position, destination) > BLOCK_SIZE * 4.5)
			{
				texturePos.y = 2;
			}
			else
			{
				if (isMovingType == 1)
				{
					texturePos.y = 1;
				}else
				{
					texturePos.y = 2;
				}

			}

			glm::vec2 dir = destination - startPos;
			dir = glm::normalize(dir);
			dir *= deltaTime * birdSpeed;
			position += dir;
		}else
		{
			position = destination;

			texturePos.y = 0;
			
			if (isMovingType == 2)
			{
				showing = 0;
			}
			
			isMovingType = 0;
			//todo clamp pos
		}

	}else
	{

		changeTime -= deltaTime;
		if(changeTime <= 0)
		{
			changeTime = rand() % 5 + 3;
			if(texturePos.y == 0)
			{
				texturePos.y = 3;
			}else
			{
				texturePos.y = 0;
			}

		}


		//todo animate stationary bird
	}

}

void Bird::startMove(glm::vec2 start, glm::vec2 dest)
{

	showing = true;
	position = start;
	startPos = start;
	destination = dest;
	isMovingType = 1;

}

void Bird::startEndMove(glm::vec2 start, glm::vec2 dest)
{
	startMove(start, dest);
	isMovingType = 2;
}

void Bird::draw(gl2d::Renderer2D & renderer, float deltaTime, gl2d::Texture t, glm::vec2 playerPos)
{

	if (!showing)
	{
		return;
	}

	//animate
	frameTime -= deltaTime;

	if (frameTime <= 0)
	{
		frameTime += birdFrameDuration;
		texturePos.x++;
	}
	texturePos.x %= 4;

	auto size = t.GetSize();
	gl2d::TextureAtlasPadding ta(4, 4, size.x, size.y);

	bool rotate = 0;
	if (playerPos.x < position.x)
	{
		rotate = true;
	}
	else
	{
		rotate = false;
	}

	if (isMovingType == 2)
	{
		rotate = !rotate;
	}

	float perc = getShowPerc();

	if (isMovingType == 1)
	{
		perc = 1 - perc;
	}
	else if (isMovingType == 0)
	{
		perc = 1;
	}


	renderer.renderRectangle({ position,BLOCK_SIZE,BLOCK_SIZE },
		{1,1,1,perc},		
		{}, 0, t, ta.get(texturePos.x, texturePos.y, rotate)
	);



}

float Bird::getShowPerc()
{

	return glm::distance(position, destination) / glm::distance(startPos, destination);

}

#pragma endregion

void Butterfly::draw(gl2d::Renderer2D& renderer, float deltaTime,
	gl2d::Texture butterfly, gl2d::Texture firefly)
{
	gl2d::TextureAtlasPadding ta;
	gl2d::Texture t;

	if (type == butterfyType)
	{
		auto size = butterfly.GetSize();
		ta = gl2d::TextureAtlasPadding(4, 4, size.x, size.y);
		t = butterfly;
	}
	else if(type == fireFlyType)
	{
		auto size = firefly.GetSize();
		ta = gl2d::TextureAtlasPadding(4, 1, size.x, size.y);
		t = firefly;
		texturePos.y = 0;
		light = 1;
	}

	//animate
	frameTime -= deltaTime;

	if (frameTime <= 0)
	{
		frameTime += 0.15; //animation speed
		texturePos.x++;
	}
	texturePos.x %= 4;

	renderer.renderRectangle({ position,BLOCK_SIZE,BLOCK_SIZE },
		{ 1,1,1,light },
		{}, 0, t, ta.get(texturePos.x, texturePos.y, facingLeft)
	);

}

void Butterfly::updateMove(float deltaTime, MapData &mapData)
{
	float xMaxSpeed = 1.5f * BLOCK_SIZE;
	float yMaxSpeed = 0.7f * BLOCK_SIZE;

	timeTillChangeDir -= deltaTime;

	if (timeTillChangeDir <= 0)
	{
		timeTillChangeDir = (rand()%400 + 300)/100.f; 

		direction = { ((rand() % 200 - 100) * xMaxSpeed) / 100.f, ((rand() % 200 - 100) * yMaxSpeed) / 100.f };
	}
	
	position += direction * deltaTime;
	
	if (isCollidable(mapData.get((position.x+4)/ BLOCK_SIZE,
		(position.y+4)/ BLOCK_SIZE).type)
		)
	{
		direction *= -1;
		position += direction * deltaTime * 3.f;

	}

	if(glm::distance(position, anchor) > BLOCK_SIZE*8)
	{
		timeTillChangeDir = (rand() % 300 + 300) / 100.f;

		direction = (glm::normalize(anchor - position)) * (float)xMaxSpeed * (rand() % 100 / 100.f);
		
	}


	facingLeft = (direction.x > 0) ? 0 : 1;

}

static int p = 0;

void Butterfly::create()
{
	texturePos.y = rand() % 4;
	//texturePos.y = p++;
	//p %= 4;
}

void Craw::draw(gl2d::Renderer2D& renderer, float deltaTime, gl2d::Texture t)
{	
	//animate
	frameTime -= deltaTime;

	if (frameTime <= 0)
	{
		frameTime += 0.15; //animation speed
		texturePos++;
	}
	texturePos %= 4;

	auto size = t.GetSize();
	gl2d::TextureAtlasPadding ta(4, 2, size.x, size.y);
	
	int goingDown = (int)(direction.y > 0.6f * BLOCK_SIZE);

	renderer.renderRectangle({ position,BLOCK_SIZE,BLOCK_SIZE },
		{ 1,1,1,light },
		{}, 0, t, ta.get(texturePos, goingDown, facingLeft)
	);

}

void Craw::updateMove(float deltaTime, glm::vec2 playerPos, MapData& mapData)
{
	float xMaxSpeed = 4.f * BLOCK_SIZE;
	float yMaxSpeed = 4.f * BLOCK_SIZE;

	auto getAxSpeed = [xMaxSpeed]()
	{
		return xMaxSpeed * (0.8f + (rand() % 100) / 100.f * 0.2f);
	};

	float attackRadius = BLOCK_SIZE * 10.f;
	float dist = glm::distance(playerPos, position);
	if (dist <= attackRadius && dist > 0.1 * BLOCK_SIZE && playerPos.y >= anchor.y)
	{
		if(attackState == 0)
		{
			
			timeTillAttackPlayer -= deltaTime;
			
			if(timeTillAttackPlayer < 0)
			{
				attackState = 1;
				timeTillAttackPlayer = ((rand() % 100) / 100.f) * 1.5 + 1.f;
				timeTillChangeDir = (rand() % 200 + 200) / 100.f;

				direction = playerPos - position;
				direction = glm::normalize(direction);
				direction *= getAxSpeed() * 2;

			}
		}


	}
	else
	{
		timeTillAttackPlayer = ((rand() % 100) / 100.f) * 1.5 + 1.f;
		attackState = 0;
	}

	

	timeTillChangeDir -= deltaTime;

	if (timeTillChangeDir <= 0 )
	{
		attackState == 0;
		timeTillChangeDir = (rand() % 400 + 300) / 100.f;

		direction = { getAxSpeed() , 0 };
	}

	//try return
	if (attackState != 1)
	{
		if (position.y > anchor.y)
		{
			direction.y = -yMaxSpeed * (0.7f + (rand() % 100) / 100.f * 0.3f);
		}
		else if(position.y < anchor.y - 0.5 * BLOCK_SIZE)
		{
			direction.y = yMaxSpeed * (0.7f + (rand() % 100) / 100.f * 0.3f);
		}
		else
		{
			direction.y = 0;
		}

	}


	//avoid collision
	if (isCollidable(mapData.get((position.x + 4) / BLOCK_SIZE,
		(position.y + 4) / BLOCK_SIZE).type)
		)
	{
		direction *= -1;
		position += direction * deltaTime;
		timeTillChangeDir = (rand() % 300 + 300) / 100.f;
	}

	if (attackState == 0)
	{
		if (glm::distance(position, anchor) > BLOCK_SIZE * 8)
		{
			timeTillChangeDir = (rand() % 200 + 200) / 100.f;

			direction = (glm::normalize(anchor - position)) * getAxSpeed();
			attackState = 0;
		}
	}
	else
	{
		if (glm::distance(position, anchor) > BLOCK_SIZE * 11)
		{
			timeTillChangeDir = (rand() % 200 + 200) / 100.f;

			direction = (glm::normalize(anchor - position)) * getAxSpeed();
			attackState = 0;
		}
	}

	

	position += direction * deltaTime;


	facingLeft = (direction.x > 0) ? 0 : 1;

}
