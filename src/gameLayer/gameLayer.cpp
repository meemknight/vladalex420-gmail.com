#include "gameLayer.h"
#include "gl2d.h"
#include "mapRenderer.h"
#include "mapData.h"
#include "input.h"
#include "Ui.h"
#include "Particle.h"
#include "DialogInteraction.h"
#include <algorithm>
#include <string>
#include <sstream>
#include "menu.h"
#include "Settings.h"
#include "Sound.h"
#include "gameMath.h"
#include "levelSelector.h"
#include <ctime>

extern float gravitationalAcceleration;
extern float jumpSpeed;
extern float jumpFromWallSpeed;
extern float velocityClamp;
//extern float drag;
//extern float strafeSpeed = 10;
extern float runSpeed;
extern float airRunSpeed;
extern float grabMargin;
extern float notGrabTimeVal;
extern bool snapWallGrab;

extern float fullScreenZoom;

extern gl2d::internal::ShaderProgram maskShader;
extern GLint maskSamplerUniform;

extern int currentSettingsMenu;

gl2d::Renderer2D renderer2d;
//gl2d::Renderer2D stencilRenderer2d;

#pragma region music

//music


SoundManager soundManager;

#pragma endregion

MapRenderer mapRenderer;
MapData mapData;
Entity player;


MapData levelPreviewMapData;

#pragma region Textures

gl2d::Texture sprites;
gl2d::Texture characterSprite;
gl2d::Texture arrowSprite;
gl2d::TextureAtlasPadding arrowTextureAtlas;
gl2d::Texture particlesSprite;
gl2d::Texture crackTexture;
gl2d::Texture birdTexture;
gl2d::Texture butterflyTexture;
gl2d::Texture fireFlyTexture;
gl2d::Texture crawTexture;


gl2d::Texture uiItch;
gl2d::Texture uiMusic;
gl2d::Texture uiArt;
gl2d::Texture uiDialogBox;
gl2d::Texture uiBackArrow;
gl2d::Texture uiImages;
gl2d::Texture uiButtons;

textureDataWithUV tDDCaracter;
textureDataWithUV tDDCaracterAnnoyed;
textureDataWithUV tDDCaracterSurprized;
textureDataWithUV tDDBird;

#pragma endregion

std::vector<Arrow> arrows;

std::vector<Pickup> pickups;

gl2d::Font font;

int currentArrow = Arrow::normalArrow;

const float arrowPickupCullDown = 5;

glm::ivec2 playerSpawnPos = { 0,0 };

Particle jumpParticle;
std::vector<Particle>crackParticles;

DialogInteraction currentDialog;

// -2 if is main menu
int currentLevel = -2;

float jumpDelayTime = 0;

//todo enum here for stuff
static int ingameMenuMainPage = 1;
bool inGameMenu = 0;
bool inGameControllsMenu = 0;

struct ArrowItem
{
	int type;
	int count;
	int maxCount;
};

//this vector should always have all arrows in order (and all of them there)
std::vector <ArrowItem> inventory;
std::vector <ArrowItem> actualInventorty;

std::vector <LightSource> wallLights;
std::vector <LightSource> levelPreviewWallLights;

const float playerLight = 6;
const float fireFlyLight = 3;

//this is also used to tell if the player died
float lightPerc = 1;

Bird bird;


void respawn();

int blueChanged = 0, redChanged = 0, grayChanged = 0;
glm::ivec2 litTorchesThisGame[256] = {};
int litTorchesThisGameCount = 0;

void loadLevel(glm::ivec2 spawn = { 0, 0 }, bool setSpawn = 0)
{
	ingameMenuMainPage = 1;
	inGameMenu = 0;
	//currentDialog.dialogData.push_back({{ "Dialog, Sample.\nFraze 2. Text3." }, textureDataForDialog["character"]});
	//currentDialog.dialogData.push_back({ { "Nu stiu ce s azic.\nFraze 2. Text3." }, textureDataForDialog["character"] });
	//currentDialog.dialogData.push_back({ {"Dialog, Sample3.\n epic moment aici."}, textureDataForDialog["character"] });

	//currentDialog.start();

	inventory.clear();
	//this vector should always have all arrows in order (and all of them there)
	inventory.push_back({ 0,0,3 });
	inventory.push_back({ 1,0,3 });
	inventory.push_back({ 2,0,3 });
	inventory.push_back({ 3,0,3 });
	inventory.push_back({ 4,0,3 });

	wallLights.clear();


	pickups.clear();
	arrows.clear();

	//pickups.push_back({ 4, 4, 1 });

	if (currentLevel == -2)
	{
		mapData.cleanup();
		return;
	}

	currentDialog.resetDialogData();
	currentDialog = {};
	bird = Bird{};


	setupMap(mapData, currentLevel);

	for (int y = 0; y < mapData.h; y++)
		for (int x = 0; x < mapData.w; x++)
		{
			if (mapData.get(x, y).type == Block::flagUp)
			{
				if (setSpawn)
				{
					mapData.get(x, y).type = Block::flagDown;
				}
				else
				{
					playerSpawnPos = { x,y };
				}
			}

			if (mapData.get(x, y).type == Block::woddenArrow)
			{
				pickups.push_back({ x, y, 0 });
				mapData.get(x, y).type = Block::none;
			}
			if (mapData.get(x, y).type == Block::fireArrow)
			{
				pickups.push_back({ x, y, 1 });
				mapData.get(x, y).type = Block::none;
			}
			if (mapData.get(x, y).type == Block::slimeArrow)
			{
				pickups.push_back({ x, y, 2 });
				mapData.get(x, y).type = Block::none;
			}
			if (mapData.get(x, y).type == Block::keyArrow)
			{
				pickups.push_back({ x, y, 3 });
				mapData.get(x, y).type = Block::none;
			}if (mapData.get(x, y).type == Block::bombArrow)
			{
				pickups.push_back({ x, y, 4 });
				mapData.get(x, y).type = Block::none;
			}
		}

	if (setSpawn)
	{
		playerSpawnPos = spawn;
	}

	player.pos = { BLOCK_SIZE * playerSpawnPos.x, BLOCK_SIZE * playerSpawnPos.y };
	//player.updateMove();
	player.lastPos = player.pos;

	//todo refactor
	player.dimensions = { 7, 7 };
	player.dying = 0;
	player.lockMovementDie = 0;
	lightPerc = 1;
	player.velocity = {};
	player.isExitingLevel = -1;
	player.wallGrab = 0;
	player.iceGrab = 0;
	player.isSittingOnIce = 0;
	player.notCrawTime = 0;


#pragma region setup light sources
	wallLights.clear();

	for (int y = 0; y < mapData.h; y++)
		for (int x = 0; x < mapData.w; x++)
		{
			if (isLitTorch(mapData.get(x, y).type))
			{
				wallLights.push_back({ {x,y}, 0, mapData.getTorchLightIntensity(x,y) });
			}
			else if (unLitTorch(mapData.get(x, y).type))
			{
				auto data = mapData.getTorchData(x, y);
				wallLights.push_back(LightSource({ x,y }, 0, 0, data.xBox, data.yBox));
			}
		}
#pragma endregion


	soundManager.setMusicPositions(mapData);

	//memset(litTorchesThisGame, 0, sizeof(litTorchesThisGame));
	//litTorchesThisGameCount = 0;

	saveState(playerSpawnPos, currentLevel, mapData.dialogs, 0, 0, 0, litTorchesThisGame, litTorchesThisGameCount);
	respawn();
}

void respawn()
{
	inventory.clear();
	//this vector should always have all arrows in order (and all of them there)
	inventory.push_back({ 0,0,3 });
	inventory.push_back({ 1,0,3 });
	inventory.push_back({ 2,0,3 });
	inventory.push_back({ 3,0,3 });

	arrows.clear();

	player.pos = { BLOCK_SIZE * playerSpawnPos.x, BLOCK_SIZE * playerSpawnPos.y + 1 };
	player.updateMove(0);
	player.dimensions = { 7, 7 };
	player.dying = 0;
	player.lockMovementDie = 0;
	lightPerc = 1;
	player.velocity = {};
	player.wallGrab = 0;
	player.iceGrab = 0;
	player.isSittingOnIce = 0;
	player.grounded = 1;
	player.canJump = 1;
	player.hasTouchGround = 1;
	player.movingRight = 1;
	player.notCrawTime = 0;

	//reset pickups
	for (auto &i : pickups)
	{
		i.cullDown = 0;
	}

	glm::vec2 camPos = player.pos + (player.dimensions / 2.f);
	camPos -= glm::vec2(renderer2d.windowW / 2.f, renderer2d.windowH / 2.f);
	renderer2d.currentCamera.position = camPos;
}

bool initGame()
{
	srand(time(0));

	font.createFromFile(RESOURCES_PATH "font.TTF");

	glClearColor(BACKGROUNDF_R, BACKGROUNDF_G, BACKGROUNDF_B, 1.f);

	renderer2d.create();
	//stencilRenderer2d.create();
	//if (music.openFromFile("ding.flac"))
	//music.play();
	ShaderProgram sp{ RESOURCES_PATH "blocks.vert", RESOURCES_PATH "blocks.frag" };

	mapRenderer.init(sp);
	sprites.loadFromFileWithPixelPadding(RESOURCES_PATH "sprites.png", BLOCK_SIZE);

	mapRenderer.sprites = sprites;
	mapRenderer.upTexture.loadFromFile(RESOURCES_PATH "top.png");
	mapRenderer.downTexture.loadFromFile(RESOURCES_PATH "bottom.png");
	mapRenderer.leftTexture.loadFromFile(RESOURCES_PATH "left.png");
	mapRenderer.rightTexture.loadFromFile(RESOURCES_PATH "right.png");

	characterSprite.loadFromFileWithPixelPadding(RESOURCES_PATH "character.png", 8);

	//arrowSprite.loadFromFile(RESOURCES_PATH "arrow.png");
	arrowSprite.loadFromFileWithPixelPadding(RESOURCES_PATH "arrow.png", 7);
	{
		auto tSize = arrowSprite.GetSize();
		arrowTextureAtlas = gl2d::TextureAtlasPadding(5, 1, tSize.x, tSize.y);
	}

	particlesSprite.loadFromFileWithPixelPadding(RESOURCES_PATH "particles.png", 8);
	crackTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "crackAnim.png", 8);
	birdTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "bird.png", 8);
	butterflyTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "butterfly.png", 8);
	fireFlyTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "fireFly.png", 8);
	crawTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "craw.png", 8);


	uiItch.loadFromFile(RESOURCES_PATH "ui//itch.png");
	uiMusic.loadFromFile(RESOURCES_PATH "ui//music.jpg");
	uiArt.loadFromFile(RESOURCES_PATH "ui//art.png");
	uiDialogBox.loadFromFile(RESOURCES_PATH "ui//uiDialogFrame.png");
	uiBackArrow.loadFromFile(RESOURCES_PATH "ui//back.png");
	uiImages.loadFromFileWithPixelPadding(RESOURCES_PATH "ui//uiImages.png", 8);
	uiButtons.loadFromFileWithPixelPadding(RESOURCES_PATH "ui//uiButtons.png", 16);

	gl2d::TextureAtlasPadding uiImagesAtlas(4, 1, uiImages.GetSize().x, uiImages.GetSize().y);

	tDDCaracter = { uiImages, uiImagesAtlas.get(0,0) };
	tDDCaracterAnnoyed = { uiImages, uiImagesAtlas.get(1,0) };
	tDDCaracterSurprized = { uiImages, uiImagesAtlas.get(2,0) };
	tDDBird = { uiImages, uiImagesAtlas.get(3,0) };


	const unsigned char buff[] =
	{
		BACKGROUND_R,
		BACKGROUND_G,
		BACKGROUND_B,
		0xff
	};

	jumpParticle.animCount = 3;

	arrows.reserve(20);


	{//music

		soundManager.loadMusic();
	}

	//if (loadLevelFromLastState(currentLevel, playerSpawnPos))
	//{
	//	glm::ivec2 i = playerSpawnPos;
	//	loadLevel();
	//	playerSpawnPos = i;
	//	respawn();
	//}else
	{
		currentLevel = -2;
	}

	//loadProgress(maxLevel);

	settings::loadSettings();

	initLevelSelectorData();

	loadPreviewMap(levelPreviewMapData);
#pragma region setup light sources for the level preview
	levelPreviewWallLights.clear();

	for (int y = 0; y < levelPreviewMapData.h; y++)
		for (int x = 0; x < levelPreviewMapData.w; x++)
		{
			if (isLitTorch(levelPreviewMapData.get(x, y).type))
			{
				levelPreviewWallLights.push_back({ {x,y}, 0, levelPreviewMapData.getTorchLightIntensity(x,y) });
			}
			else if (unLitTorch(levelPreviewMapData.get(x, y).type))
			{
				auto data = levelPreviewMapData.getTorchData(x, y);
				levelPreviewWallLights.push_back(LightSource({ x,y }, 0, 0, data.xBox, data.yBox));
			}
		}

	levelPreviewMapData.clearColorData();

	for (auto &i : levelPreviewWallLights)
	{
		simulateLightSpot({ i.pos.x * BLOCK_SIZE + BLOCK_SIZE / 2,i.pos.y * BLOCK_SIZE + BLOCK_SIZE / 2 },
		i.intensity * lightPerc, levelPreviewMapData, nullptr, nullptr, nullptr, nullptr);

	}
	

#pragma endregion


	return true;
}

enum MenuState:int
{
	mainMenu = 1,
	levelSelector,
	settingsMenu,
	controlls,

}; int menuState = MenuState::mainMenu;


bool gameLogic(float deltaTime)
{

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	int w, h;
	w = platform::getWindowSizeX();
	h = platform::getWindowSizeY();
	glViewport(0, 0, w, h);
	renderer2d.updateWindowMetrics(w, h);

	if (input::isControllerInput() || (currentArrow == -1 && currentLevel != -2 && !inGameMenu))
	{
		platform::showMouse(false);
	}
	else
	{
		platform::showMouse(true);
	}

	float cameraZoom = (w / (float)600) * settings::getZoom();


#pragma region MyRegion

	static int selectedLevel;

	if (input::isKeyReleased(input::Buttons::esc) && menuState != MenuState::settingsMenu)
	{
		menuState = MenuState::mainMenu;
	}

	//main menu
	if (currentLevel == -2)
	{

		soundManager.stoppMusic();


#pragma region render preview level
		if(menuState != MenuState::levelSelector)
		{

			renderer2d.currentCamera.zoom = cameraZoom;
			glm::vec2 camPos = glm::vec2(20, 20) * glm::vec2(BLOCK_SIZE);
			camPos -= glm::vec2(w / 2.f, h / 2.f);
			renderer2d.currentCamera.position = camPos;
			//renderer2d.currentCamera.position = glm::vec2( 20,20 );

			static int animPos;
			static float timePassed;
			timePassed += deltaTime;
			while (timePassed > 0.17)
			{
				timePassed -= 0.17;
				animPos++;
			}
			animPos %= 4;

			mapRenderer.drawFromMapData(renderer2d, levelPreviewMapData, deltaTime, animPos);
		}
#pragma endregion


		renderer2d.currentCamera = gl2d::cameraCreateDefault();

		if (menuState == MenuState::mainMenu)
		{

			bool playButton = 0;
			bool levelSelectButton = 0;
			bool settingsButton = 0;
			bool exitButton = 0;
			bool controllsButton = 0;

			menu::startMenu(6);

			menu::uninteractableCentreText("Midnight arrow");
			//if (getSavedLevelNumber() >= 0) //todo
			{ menu::interactableText("Continue journey", &playButton); }

			menu::interactableText("Select zone", &levelSelectButton);
			menu::interactableText("Settings", &settingsButton);
			menu::interactableText("Controlls", &controllsButton);
			menu::interactableText("Exit", &exitButton);

			menu::endMenu(renderer2d, {}, uiBackArrow, font, nullptr, deltaTime);

			if (playButton) //load from last state
			{

				glm::ivec2 dialogPos[20]; //note don't change this number

				blueChanged = 0;
				redChanged = 0;
				grayChanged = 0;

				if (loadLevelFromLastState(currentLevel, playerSpawnPos, dialogPos, blueChanged, redChanged, grayChanged
					,litTorchesThisGame, litTorchesThisGameCount)
					&& currentLevel != -2
					)
				{

					glm::ivec2 i = playerSpawnPos;
					loadLevel(i, true);

					for (int i = 0; i < 20; i++)
					{
						if (dialogPos[i].x == -1)
						{
							break;
						}

						auto it = mapData.dialogs.find(dialogPos[i]);

						if (it != mapData.dialogs.end())
						{
							it->second.hasShown = true;
						}

					}

					for (int i = 0; i < litTorchesThisGameCount; i++)
					{
						int x = litTorchesThisGame[i].x;
						int y = litTorchesThisGame[i].y;

						auto it = std::find_if(wallLights.begin(), wallLights.end(), [x, y](LightSource &ls)
						{
							return ls.pos.x == x && ls.pos.y == y;
						});

						if (it != wallLights.end())
						{
							it->animationDuration = 0;
							it->intensity = mapData.getTorchLightIntensity(x, y);
							mapData.get(x, y).type++; //lit texture up
						}

					}

					for (int y = 0; y < mapData.h; y++)
						for (int x = 0; x < mapData.w; x++)
						{
							auto &b = mapData.get(x, y);

							if (blueChanged)
							{
								if (isBlueSolid(b.type))
								{
									b.type++;
								}
								else
									if (isBlueNoSolid(b.type))
									{
										b.type--;
									}
							}

							if (redChanged)
							{
								if (isRedSolid(b.type))
								{
									b.type++;
								}
								else
									if (isRedNoSolid(b.type))
									{
										b.type--;
									}
							}

							if (grayChanged)
							{
								if (b.type == Block::fenceSolid)
								{
									b.type++;
								}
								else
									if (b.type == Block::fenceNoSolid)
									{
										b.type--;
									}
							}
						}

					mapData.setNeighbors();
				}
				else
				{
					currentLevel = 0;
					loadLevel();
					blueChanged = 0; redChanged = 0; grayChanged = 0;
					memset(litTorchesThisGame, 0, sizeof(litTorchesThisGame));
					litTorchesThisGameCount = 0;
				}

			}

			if (levelSelectButton)
			{
				menuState = MenuState::levelSelector;
				enterMenu(); //init stuff
				selectedLevel = 0;

			}

			if (settingsButton)
			{
				menuState = MenuState::settingsMenu;
				settings::setMainSettingsPage();
			}

			if (controllsButton) 
			{
				menuState = MenuState::controlls;
			}

			if (exitButton)
			{
				return 0;
			}

		}
		else if (menuState == MenuState::levelSelector)
		{

			int l = levelSelectorMenu(deltaTime, renderer2d, uiDialogBox, font);

			if (l > -1) 
			{
				currentLevel = l;
				loadLevel();
				memset(litTorchesThisGame, 0, sizeof(litTorchesThisGame));
				litTorchesThisGameCount = 0;
			}
			
		}
		else if (menuState == MenuState::settingsMenu)
		{
			settings::displaySettings(renderer2d, deltaTime);

			if (currentSettingsMenu == 0)
			{
				menuState = MenuState::mainMenu;
			}

			/*		https://wuxia.itch.io/
			if (isButtonReleased(p, button1)) {system("start https://wuxia.itch.io/"); };
			if (isButtonReleased(p, button2)) {system("start https://www.youtube.com/channel/UCEXX5i6961zc4-L8thTctBg");};
			if (isButtonReleased(p, button3)) {system("start https://itch.io/profile/adamatomic"); };
			*/
		}
		else if (menuState == MenuState::controlls)
		{
			if(input::viewControllsPage(renderer2d, uiDialogBox, uiBackArrow, deltaTime))
			{
				menuState = MenuState::mainMenu;
			}

		}


		renderer2d.flush();
		return 1;
	}  ////////////////////////////////////////// end main menu

#pragma endregion

	float copyDeltaTime = deltaTime;
	if (inGameMenu)
	{
		deltaTime = 0;
	}


	//if (platform::isKeyPressedOn('T'))
	//{
	//	loadLevel();
	//}

	if (player.dying || player.isExitingLevel != -1)
	{
		lightPerc -= deltaTime * 1.8;
		if (lightPerc < 0)
		{
			if (player.dying)
			{
				respawn();
			}
			else
			{
				currentLevel = player.isExitingLevel;

				if (currentLevel == -2)
				{
					mapData.cleanup();
					loadLevel(); //todo other function for close level

					return 1;
				}
				else
				{
					loadLevel();
				}

			}
		}
	}

#pragma region music


	soundManager.updateSoundVolume();

	soundManager.setMusicAndEffectVolume(player.pos);
	soundManager.updateSoundTransation(deltaTime);

#pragma endregion



#pragma region controlls

	//if (settings::isFullScreen())
	//{
	//	renderer2d.currentCamera.zoom = fullScreenZoom * 4;
	//}
	//else
	{

		renderer2d.currentCamera.zoom = cameraZoom ;
	}


	if (!currentDialog.blockMovement() && !inGameMenu)
	{
		if (player.wallGrab == 0)
		{
			if (player.grounded)
			{
				player.run(deltaTime * input::getMoveDir(), deltaTime);
			}
			else
			{
				player.airRun(deltaTime * input::getMoveDir());
			}
		}

		if (jumpDelayTime > 0)
		{
			jumpDelayTime -= deltaTime;
		}
		if (jumpDelayTime < 0) { jumpDelayTime = -1; }

		if (player.isExitingLevel == -1 && (input::isKeyPressedOn(input::Buttons::jump) || jumpDelayTime > 0))
		{
			if (input::isKeyPressedOn(input::Buttons::jump) && player.wallGrab == 0)
			{
				jumpDelayTime = 0.2;
			}

			if (player.wallGrab == 0)
			{
				if (player.canJump)
				{
					jumpDelayTime = 0;
					player.jump();
					player.canJump = 0;
					jumpParticle.set(player.pos, 0, player.movingRight);
				}
			}
			else if (player.wallGrab == 1)
			{
				jumpDelayTime = 0;
				player.strafe(-1);
				player.jumpFromWall();
				player.wallGrab = 0;
				player.redGrab = 0;
				player.grayGrab = 0;
				player.blueGrab = 0;
				player.iceGrab = 0;
				jumpParticle.set(player.pos, 1, !player.movingRight);

			}
			else if (player.wallGrab == -1)
			{
				jumpDelayTime = 0;
				player.strafe(1);
				player.jumpFromWall();
				player.wallGrab = 0;
				player.iceGrab = 0;
				player.redGrab = 0;
				player.grayGrab = 0;
				player.blueGrab = 0;
				jumpParticle.set(player.pos, 1, !player.movingRight);

			}
		}

		if (!input::isKeyHeld(input::Buttons::jump) && !input::isKeyPressedOn(input::Buttons::jump))
		{
			if (player.velocity.y < 0)
			{
				player.velocity.y *= 0.4;
			}
		}

		if (input::isKeyPressedOn(input::Buttons::shoot) && currentArrow > -1 && !player.dying
			&& player.isExitingLevel == -1 && !inGameMenu)
		{
			player.idleTime = 0;
			{

				for (auto &i : inventory)
				{
					if (i.type == actualInventorty[currentArrow].type)
					{

						i.count--;
						break;
					}
				}

				Arrow a;
				a.type = (Arrow::ArrowTypes)actualInventorty[currentArrow].type;
				a.pos = player.pos + glm::vec2(player.dimensions.x / 2, player.dimensions.y / 2);
				a.shootDir = input::getShootDir({ w / 2,h / 2 });
				a.pos.x += a.shootDir.x * BLOCK_SIZE * 0.9;
				a.pos.y += a.shootDir.y * BLOCK_SIZE * 0.9;
				arrows.push_back(a);
			}
		}


		if (input::isKeyHeld(input::Buttons::down))
		{
			player.wallGrab = 0;
			player.iceGrab = 0;
		}
		else
		{
			player.checkWall(mapData, input::getMoveDir());
		}


	#pragma region jump prticle


		{
			static bool animateFall;
			static float animReloadTime;
			if (player.velocity.y >= 230)
			{
				if (animateFall)
				{
					jumpParticle.set(player.pos, 2, player.movingRight);
					animateFall = 0;
					animReloadTime = 0.2;
				}
				animReloadTime -= deltaTime;
				if (animReloadTime <= 0)
				{
					animateFall = 1;
					animReloadTime = 0.92;
				}
			}
			else
			{
				animateFall = 1;
			}
		}

	#pragma endregion


	}
	else
	{
		player.idleTime = 0;
		jumpDelayTime = 0;
	}

	if (!platform::isFocused())
	{
		float notGrabTime = 0;
		player.idleTime = 0;
		jumpDelayTime = 0;
	}

	renderer2d.currentCamera.follow(player.pos + (player.dimensions / 2.f), deltaTime * 100, 4 * BLOCK_SIZE, renderer2d.windowW, renderer2d.windowH);

	if (player.notCrawTime <= 0)
	{
		player.notCrawTime = 0;
		bool hit = 0;

		for (auto& i : mapData.craws)
		{
			if (glm::distance(player.pos+(player.dimensions/2.f), i.position) < 0.8 * BLOCK_SIZE && player.notCrawTime <= 0)
			{
				player.notCrawTime = 0.1;
				player.velocity.y -= 3 * BLOCK_SIZE;
				hit = 1;

				player.wallGrab = 0;
				player.redGrab = 0;
				player.grayGrab = 0;
				player.blueGrab = 0;
				player.iceGrab = 0;

				if (player.pos.x < i.position.x)
				{
					//player.jumpFromWall();
					player.velocity.x = -16 * BLOCK_SIZE;
				}else
				{
					//player.jumpFromWall();
					player.velocity.x = 16 * BLOCK_SIZE;
				}

				
			}

		}

		//if (!hit)
		{
			player.applyGravity(deltaTime);
		}

	}else
	{
		player.notCrawTime -= deltaTime;
		if (player.notCrawTime <= 0)
		{
			player.notCrawTime = 0;
		}

		player.applyGravity(deltaTime);

	}

	player.applyVelocity(deltaTime);

	player.resolveConstrains(mapData);

	player.updateMove(deltaTime);

	player.checkGrounded(mapData, deltaTime);

	//ilog(player.velocity.x);

	if (!currentDialog.blockMovement() && !inGameMenu)
	{


	#pragma region inventory

		if (!platform::isFocused())
		{
			currentArrow = -1;
		}

		if (input::isKeyPressedOn(input::Buttons::swapLeft))
		{
			currentArrow--;
		}
		else if (input::isKeyPressedOn(input::Buttons::swapRight))
		{
			currentArrow++;
		}

		if (currentArrow < -1)
		{
			currentArrow = actualInventorty.size() - 1;
		}

		if (currentArrow >= actualInventorty.size())
		{
			currentArrow = -1;
		}

		if (actualInventorty.size() == 0)
		{
			currentArrow = -1;
		}

	#pragma endregion


	}
	else
	{
		currentArrow = -1;
	}


#pragma endregion


	actualInventorty.clear();
	for (auto i : inventory)
	{
		if (i.count)
		{
			actualInventorty.push_back(i);
		}
	}

	//stencilRenderer2d.currentCamera = renderer2d.currentCamera;


	//mapRenderer.addBlock(renderer2d.toScreen({ 100,100,100,100 }), { 0,1,1,0 }, {1,1,1,1});
	//mapRenderer.render();

#pragma region lights

	mapData.clearColorData();

	//player
	simulateLightSpot(player.pos + glm::vec2(player.dimensions.x / 2, player.dimensions.y / 2),
		playerLight * lightPerc, mapData, &arrows, &pickups, &mapData.butterflies, &mapData.craws);


	//fireFlies
	for (auto& i : mapData.butterflies)
	{
		if (i.type == i.fireFlyType)
		{
			simulateLightSpot(i.position,
				fireFlyLight * lightPerc, mapData, &arrows, &pickups, &mapData.butterflies, &mapData.craws);
		}
	}


	bool isInRedBlock = 0;
	bool isInBlueBlock = 0;
	bool isInGrayBlock = 0;

	//the big for
	{
		int minX = 0;
		int minY = 0;
		int maxX = mapData.w;
		int maxY = mapData.h;

		minX = (player.pos.x) / BLOCK_SIZE;
		maxX = (player.pos.x + player.dimensions.x) / BLOCK_SIZE;

		minY = (player.pos.y - 2) / BLOCK_SIZE;
		maxY = (player.pos.y + player.dimensions.y - 1) / BLOCK_SIZE;

		minX = std::max(0, minX);
		minY = std::max(0, minY);
		maxX = std::min(mapData.w-1, maxX);
		maxY = std::min(mapData.h-1, maxY);

		int maxYplus = std::min(mapData.h-1, maxY+2);

		bool playedGrassSound = 0;
		static float grassTimeDelay;

		//for things that should be easier to touch
		for (int y = minY; y <= maxYplus; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				auto& g = mapData.get(x, y);
				if (g.type == Block::flagDown)
				{
					if (mapData.get(playerSpawnPos.x, playerSpawnPos.y).type == Block::flagUp)
					{
						mapData.get(playerSpawnPos.x, playerSpawnPos.y).type = Block::flagDown;
					}

					saveState({ x,y }, currentLevel, mapData.dialogs, blueChanged, redChanged, grayChanged
						, litTorchesThisGame, litTorchesThisGameCount);

					playerSpawnPos = { x,y };
					g.type = Block::flagUp;
				}
			}
		}


		player.iswebs = 0;
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				auto &g = mapData.get(x, y);
				if (unLitTorch(g.type))
				{
					g.type++;

					auto it = std::find_if(wallLights.begin(), wallLights.end(), [x, y](LightSource &ls)
					{
						return ls.pos.x == x && ls.pos.y == y;
					});

					if (it != wallLights.end())
					{
						it->animationDuration = it->animationStartTime;
						it->intensity = mapData.getTorchLightIntensity(x, y);
						litTorchesThisGame[litTorchesThisGameCount++] = glm::ivec2{ x,y };

					}

				}

				g.playerEntered = 1;

				if (g.type == Block::water3 || g.type == Block::lavaKill || g.type == Block::killBlock)
				{
					player.dying = 1;
				}
				else
					if (isSpike(g.type))
					{
						glm::vec2 playerP = player.pos;
						glm::vec2 blockP = { x * BLOCK_SIZE , y * BLOCK_SIZE };

						if (glm::distance(playerP, blockP) < (BLOCK_SIZE * 0.65))
						{
							player.dying = 1;
							player.lockMovementDie = 1;
						}

					}

				if (isRedNoSolid(g.type))
				{
					isInRedBlock = true;
				}
				if (isBlueNoSolid(g.type))
				{
					isInBlueBlock = true;
				}
				if (g.type == Block::fenceNoSolid)
				{
					isInGrayBlock = true;
				}

				if (g.type == Block::webBlock)
				{
					player.iswebs = true;
				}

				if (isInteractableGrass(g.type) && g.animPos == 1)
				{

					if (!playedGrassSound && grassTimeDelay <= 0)
					{
						grassTimeDelay = (rand() % 6) * 0.1 + 0.4;
						//grassTimeDelay = 0;
						playedGrassSound = 1;
						soundManager.playSound(SoundManager::soundEffects::soundEffectGrass);
					}

				}

				if (g.type == Block::levelExit)
				{

					auto iter = std::find_if(mapData.exitDataVector.begin(), mapData.exitDataVector.end(),
						[x, y](exitData &d) { return (d.pos.x == x && d.pos.y == y); });

					if (iter != mapData.exitDataVector.end())
					{
						if (input::isKeyPressedOn(input::Buttons::up))
						{
							player.isExitingLevel = iter->levelId;
							//saveProgress(iter->levelId);
							mapData.dialogs.clear();
							saveState({ -1,-1 }, iter->levelId, mapData.dialogs, 0, 0, 0
							,litTorchesThisGame, litTorchesThisGameCount);

						}
					}
					else
					{
						if (input::isKeyPressedOn(input::Buttons::up))
						{
							player.isExitingLevel = -2;
						}
					}

					//todo delegate this render text for later

					input::drawButtonWithHover(renderer2d, { BLOCK_SIZE * (x), BLOCK_SIZE * (y - 1.2) }, BLOCK_SIZE,
						input::Buttons::up);

					//renderer2d.renderText({ BLOCK_SIZE*(x), BLOCK_SIZE*(y - 1) },
					//	"Press Up to exit", font, { 1,1,1,1 }, 0.09, 4, 3, true, { 0.1,0.1,0.1,1 });
				}

			}
		}

		//this code is for objects close from the player not necessariliy \
		touching the player


		minX = (player.pos.x) / BLOCK_SIZE;
		maxX = (player.pos.x + player.dimensions.x) / BLOCK_SIZE;

		minY = (player.pos.y - 2) / BLOCK_SIZE;
		maxY = (player.pos.y + player.dimensions.y - 1) / BLOCK_SIZE;

		minX -= 2;
		minY -= 2;
		maxX += 2;
		maxY += 2;

		minX = std::max(0, minX);
		minY = std::max(0, minY);
		maxX = std::min(mapData.w, maxX);
		maxY = std::min(mapData.h, maxY);

		for (auto &i : mapData.signDataVector)
		{
			i.shouldDisplay = 0;
		}

		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				auto &g = mapData.get(x, y);

				//todo delegate this render text for later
				if (isSign(g.type))
				{
					auto iter = std::find_if(mapData.signDataVector.begin(), mapData.signDataVector.end(),
						[x, y](signData &d)->bool { return (d.pos.x == x && d.pos.y == y); });

					if (iter != mapData.signDataVector.end())
					{
						iter->shouldDisplay = 1;
					}
				}

			}
		}


		grassTimeDelay -= deltaTime;
		if (grassTimeDelay < 0) { grassTimeDelay = 0; }

		

	}

#pragma region dialogs

	if(!currentDialog.showing)
	for (auto &i : mapData.dialogs)
	{
		if (!i.second.hasShown )
		{

			glm::vec2 dist = i.first;
			dist *= BLOCK_SIZE;
			if (glm::distance(player.pos, dist) < 5 * BLOCK_SIZE)
			{

				currentDialog.resetDialogData();
				currentDialog.dialogData = i.second.data;
				if (i.second.birdPos.x >= 0 && i.second.birdPos.y >= 0)
				{
					bird.startMove(getDiagonalBirdPos({ i.second.birdPos.x * BLOCK_SIZE, i.second.birdPos.y * BLOCK_SIZE }
					, player.pos), { i.second.birdPos.x * BLOCK_SIZE, i.second.birdPos.y * BLOCK_SIZE });
				}
				currentDialog.start();
				currentDialog.hasShownPointer = &i.second.hasShown;
			}
		}
	}

#pragma endregion


	for (auto &i : wallLights)
	{

	#pragma region checkLightBoxes

		if (i.intensity == 0 && i.boxW != 0 && i.boxH != 0)
		{

			glm::vec4 box = { (i.pos.x * BLOCK_SIZE + BLOCK_SIZE / 2) - (i.boxW * BLOCK_SIZE / 2),
				(i.pos.y * BLOCK_SIZE + BLOCK_SIZE / 2) - (i.boxH * BLOCK_SIZE / 2),
				i.boxW * BLOCK_SIZE,
				i.boxH * BLOCK_SIZE,
			};

			if (aabb(glm::vec4{ player.pos, player.dimensions }, box))
			{
				auto &b = mapData.get(i.pos.x, i.pos.y);
				if (unLitTorch(b.type))
				{
					b.type++;
				}
				litTorchesThisGame[litTorchesThisGameCount++] = glm::ivec2{ i.pos.x, i.pos.y };

				i.intensity = mapData.getTorchLightIntensity(i.pos.x, i.pos.y);
				i.animationDuration = i.animationStartTime;
			}
		}

	#pragma endregion


	#pragma region light animation


		float r = i.intensity;

		if (i.animationDuration <= 0)
		{
			r = i.intensity;
		}
		else
		{
			float perc;
			float bonusPerc = 0.2;

			if (i.animationDuration > i.animationStartTime / 2.f)
			{
				perc = i.animationDuration - (i.animationStartTime / 2.f);
				perc = perc / (i.animationStartTime / 2.f);
				perc = 1 - perc;
				r *= perc * (1 + bonusPerc);
				r = std::max(r, 0.3f);
			}
			else
			{
				perc = i.animationDuration - i.animationStartTime / 2.f;
				perc = perc / (i.animationStartTime / 2.f);
				perc = perc + 1;
				r += r * (perc)*bonusPerc;
			}

			i.animationDuration -= deltaTime;

		}

	#pragma endregion

		simulateLightSpot({ i.pos.x * BLOCK_SIZE + BLOCK_SIZE / 2,i.pos.y * BLOCK_SIZE + BLOCK_SIZE / 2 },
			r * lightPerc, mapData, &arrows, &pickups, &mapData.butterflies, & mapData.craws);

	}

	for (auto &i : arrows)
	{
		if (i.type == Arrow::fireArrow)
		{
			float r = 5;

			if (i.liveTime < 5)
			{
				r *= (i.liveTime / (float)5);

				if (i.liveTime < 1)
				{
					r = 0;
				}
			}

			if (r > 0)
			{
				simulateLightSpot({ i.pos },
					r * lightPerc, mapData, &arrows, &pickups, &mapData.butterflies, & mapData.craws);
			}

		}

	}

#pragma endregion

#pragma region target
	{
		if (currentArrow >= 0 && !player.dying && player.isExitingLevel == -1 && !inGameMenu &&
			currentArrow < actualInventorty.size())
		{
			glm::vec4 color = { 1,1,1,1 };

			switch (actualInventorty[currentArrow].type)
			{
				case Arrow::normalArrow:
				color = { 0.8,0.7,0.7,1 };
				break;
				case Arrow::fireArrow:
				color = { 0.9,0.0,0.0,1 };
				break;
				case Arrow::slimeArrow:
				color = { 0.0,0.9,0.0,1 };
				break;
				case Arrow::keyArrow:
				color = { 0.4,0.4,0.4,1 };
				break;
				default:
				break;
			}

			float fine = 0.6 * BLOCK_SIZE;
			glm::vec2 pos = player.pos + glm::vec2(player.dimensions.x / 2, player.dimensions.y / 2);
			glm::vec2 dir = input::getShootDir({ w / 2,h / 2 });
			float dist = BLOCK_SIZE * playerLight;
			for (int i = fine; i < dist; i += fine)
			{
				pos += fine * dir;

				if (pos.x < 0
					|| pos.y < 0
					|| pos.x >(mapData.w) * BLOCK_SIZE
					|| pos.y >(mapData.h) * BLOCK_SIZE)
				{

				}
				else
				{
					if (isCollidableForArrows(mapData.get(pos.x / BLOCK_SIZE, pos.y / BLOCK_SIZE).type)
						|| mapData.get(pos.x / BLOCK_SIZE, pos.y / BLOCK_SIZE).type == Block::webBlock
						)
					{
						dist = i;
						break;
					}
				}

				renderer2d.renderRectangle({ pos, 1,1 }, color);
				color.w *= 0.9;
			}
		}

	}
#pragma endregion

#pragma region draw map
	{
		static int animPos;
		static float timePassed;
		timePassed += deltaTime;
		while (timePassed > 0.17)
		{
			timePassed -= 0.17;
			animPos++;
		}
		animPos %= 4;

		//glUseProgram(mapRenderer.shader.id);
		//glUniform1i(glGetUniformLocation(mapRenderer.shader.id, "u_time"), clock());

		//renderer2d.flush();
		auto c = renderer2d.currentCamera;

		mapRenderer.drawFromMapData(renderer2d, mapData, deltaTime, animPos);

		//for(int i=0; i<3; i++)
		//{
		//	renderer2d.currentCamera.zoom *= 0.99;
		//	mapRenderer.drawFromMapData(renderer2d, mapData, deltaTime, animPos);
		//	renderer2d.flush();
		//
		//}
		//renderer2d.currentCamera = c;
	}
#pragma endregion


#pragma region pickups
	{
		bool pickedUpNow = 0;

		for (auto &i : pickups)
		{
			i.draw(renderer2d, arrowSprite, arrowTextureAtlas, deltaTime);
			i.light = 0;

			if (i.colidePlayer(player) && i.cullDown <= 0)
			{
				pickedUpNow = 1;

				i.cullDown = arrowPickupCullDown;
				inventory[i.type].count = inventory[i.type].maxCount;

				soundManager.playSound(SoundManager::soundEffects::soundEffectPickUp);
			}
		}

		actualInventorty.clear();

		for (auto i : inventory)
		{
			if (i.count)
			{
				actualInventorty.push_back(i);
			}
		}

		if (actualInventorty.size() == 1 && pickedUpNow)
		{
			currentArrow = 0;
		}

	}
#pragma endregion

#pragma region particles


	for (int i = 0; i < crackParticles.size(); i++)
	{

		crackParticles[i].duration -= deltaTime;

		if (crackParticles[i].duration <= 0)
		{
			crackParticles.erase(crackParticles.begin() + i);
			i--;

		}
		else
		{
			crackParticles[i].draw(renderer2d, deltaTime, crackTexture);
		}
	}

	jumpParticle.draw(renderer2d, deltaTime, particlesSprite);

#pragma endregion

#pragma region butterflies and craws

	for(auto &i :mapData.butterflies)
	{
		i.updateMove(deltaTime, mapData);
		i.draw(renderer2d, deltaTime, butterflyTexture, fireFlyTexture);
		i.light = 0;
	}

	for (auto& i : mapData.craws) 
	{
		i.updateMove(deltaTime, player.pos, mapData);
		i.draw(renderer2d, deltaTime, crawTexture);
		i.light = 0;
	}


#pragma endregion



	player.draw(renderer2d, deltaTime, characterSprite);


#pragma region arrows
	for (auto i = 0; i < arrows.size(); i++)
	{
		auto &a = arrows[i];
		if (a.leftMap(mapData.w, mapData.h) || a.timeOut(deltaTime))
		{
			arrows.erase(arrows.begin() + i);
			i--;
			continue;
		}

		a.move(deltaTime * BLOCK_SIZE);
		a.draw(renderer2d, arrowSprite, arrowTextureAtlas);
		a.checkCollision(mapData, isInRedBlock, isInBlueBlock, isInGrayBlock, redChanged, blueChanged, grayChanged,
			litTorchesThisGame, litTorchesThisGameCount);
		a.light = 0;


		if (!a.shownAnim)
		{
			if (a.stuckInWall)
			{
				a.shownAnim = 1;
			}

			int x, y;
			x = a.pos.x / BLOCK_SIZE;
			y = a.pos.y / BLOCK_SIZE;

			if (x >= 0 && y >= 0 && x <= mapData.w && y <= mapData.h)
			{
				mapData.get(x, y).playerEntered = 1;
			}

		}
	}

#pragma endregion

#pragma region bird

	bird.update(deltaTime);
	bird.draw(renderer2d, deltaTime, birdTexture, player.pos);


#pragma endregion

#pragma region ui

	{
		auto c = renderer2d.currentCamera;
		renderer2d.currentCamera.setDefault();

		Ui::Frame f({ 0,0, w,h });

		//ui
		{
			glm::vec4 uiBox;

			if (settings::showArrowIndicators())
			{
				uiBox = Ui::Box().xLeftPerc(0.08).yBottom(-20).xDimensionPercentage(0.1f).yAspectRatio(0.5f)();
			}
			else
			{
				uiBox = Ui::Box().xLeft(20).yBottom(-20).xDimensionPercentage(0.1f).yAspectRatio(0.5f)();
			}

			Ui::Frame cornerLeft(uiBox);

			int centerCount = 1;
			int leftCount = 1;
			int rightCount = 1;

			if (actualInventorty.size() != 0)
			{

				int left = currentArrow - 1;
				if (left == -1)
				{

				}
				else if (left < -1)
				{
					if (actualInventorty.size() > 1)
					{
						leftCount = (actualInventorty.end() - 1)->count;
						left = (actualInventorty.end() - 1)->type;
					}
				}
				else
				{
					if (left < -1)
					{
						left = actualInventorty.size() - 1;
					}

					leftCount = actualInventorty[left].count;
					left = actualInventorty[left].type;
				}

				if (actualInventorty.size() == 1)
				{
					left = -1;
				}

				int center = currentArrow;
				if (center <= -1 || actualInventorty.size() <= center)
				{
					center = -1;
				}
				else
				{
					centerCount = actualInventorty[center].count; //!todo
					center = actualInventorty[center].type;
				}


				int right = currentArrow + 1;;
				if (right >= actualInventorty.size())
				{
					right = -1;
				}
				else
				{
					rightCount = actualInventorty[right].count;
					right = actualInventorty[right].type;
				}

				float angle = 45.f;


				if (left > -1)
				{
					float dim = 0.2;
					for (int i = -1; i < leftCount - 1; i++)
					{
						renderer2d.renderRectangle(
							Ui::Box().xLeft(i * 8).yCenter().yDimensionPercentage(0.7f).xAspectRatio(1)
							, { 0.4 * dim,0.4 * dim,0.4 * dim,1 }
						, {}, angle, arrowSprite, arrowTextureAtlas.get(left, 0));
						dim += 0.1;
					}
				}


				if (right > -1)
				{
					float dim = 0.2;
					for (int i = -1; i < rightCount - 1; i++)
					{
						renderer2d.renderRectangle(
							Ui::Box().xRight(i * 8).yCenter().yDimensionPercentage(0.7f).xAspectRatio(1)
							, { 0.4 * dim,0.4 * dim,0.4 * dim,1 }
						, {}, angle, arrowSprite, arrowTextureAtlas.get(right, 0));
						dim += 0.1;
					}
				}

				if (center > -1)
				{
					float dim = 1 - (centerCount / 10.f);

					for (int i = -1; i < centerCount - 1; i++)
					{

						renderer2d.renderRectangle(
							Ui::Box().xCenter(i * 10).yCenter().yDimensionPercentage(0.9f).xAspectRatio(1),
							{ dim,dim,dim,1 },
							{}, angle, arrowSprite, arrowTextureAtlas.get(center, 0));
						dim += 0.1;
					}

					//renderer2d.renderText(Ui::Box().xCenter().yBottom()(),
					//	"1/2", font, { 1,1,1,1 }, 0.5);

				}

			}

		}

		//the 2 buttons
		if (actualInventorty.size() != 0 && settings::showArrowIndicators())
		{
			float leftDim = 0.6;
			float righttDim = 0.6;

			auto left = Ui::Box().xLeft(10).yBottom(-20).xDimensionPercentage(0.04f).yAspectRatio(1.f)();
			auto right = Ui::Box().xLeftPerc(0.2).yBottom(-20).xDimensionPercentage(0.04f).yAspectRatio(1.f)();

			if (input::isKeyHeld(input::Buttons::swapLeft))
			{
				left = Ui::Box().xLeft(10).yBottom(-15).xDimensionPercentage(0.04f).yAspectRatio(1.f)();
				leftDim = 0.4;
			}

			if (input::isKeyHeld(input::Buttons::swapRight))
			{
				right = Ui::Box().xLeftPerc(0.2).yBottom(-15).xDimensionPercentage(0.04f).yAspectRatio(1.f)();
				righttDim = 0.4;
			}

			right.x += 10;

			input::drawButton(renderer2d, left, left.z, input::Buttons::swapLeft, input::isControllerInput(), leftDim, { 3.f, 3.f }, 1.f);
			input::drawButton(renderer2d, right, right.z, input::Buttons::swapRight, input::isControllerInput(), righttDim, { 3.f, 3.f }, 1.f);

		}

		renderer2d.currentCamera = c;
	}

#pragma endregion

#pragma region sign

	for (auto &iter : mapData.signDataVector)
	{
		if (iter.shouldDisplay || iter.time != 0)
		{
			const float ANIM_TIME = 0.2f;

			if (iter.shouldDisplay)
			{
				if (iter.time < ANIM_TIME)
				{
					iter.time += deltaTime;
					iter.time = std::min(iter.time, ANIM_TIME);
				}
			}
			else
			{
				if (iter.time > 0)
				{
					iter.time -= deltaTime;
					iter.time = std::max(iter.time, 0.f);
				}

			}

			float c = iter.time / ANIM_TIME;
				
			auto s = renderer2d.getTextSize(iter.text.c_str(), font, 0.09, 4, 3);
			//auto backgroundSize = s;
			//backgroundSize.y += BLOCK_SIZE;
			//
			//if(iter.button >= 0)
			//{
			//	backgroundSize.x += BLOCK_SIZE * 2;
			//}
			//
			//renderer2d.renderRectangle({ BLOCK_SIZE * (iter.pos.x) - backgroundSize.x * c * 0.5, 
			//	BLOCK_SIZE * (iter.pos.y - 2), backgroundSize.x * c, backgroundSize.y * sqrt(c) },
			//		Colors::backgroundBlue);

			//if (c >= 0.90)
			{
				//render text from sign
				renderer2d.renderText({ BLOCK_SIZE * (iter.pos.x), BLOCK_SIZE * (iter.pos.y - 1) },
					iter.text.c_str(), font, { 1,1,1,c }, 0.09, 4, 3, true, { 0.1,0.1,0.1,c });

				if (iter.button >= 0)
				{
					glm::vec2 pos = s;
					pos.y = 0;
					pos.x /= 2;
					pos += glm::vec2(BLOCK_SIZE * (iter.pos.x), BLOCK_SIZE * (iter.pos.y - 1));
					input::drawButtonWithHover(renderer2d, pos, BLOCK_SIZE, iter.button, c);
				}
			}

			

		}



	}


#pragma endregion

#pragma region dialog

	currentDialog.draw(renderer2d, w, h, deltaTime);

	if (!inGameMenu)
	{
		if ((input::isKeyReleased(input::Buttons::jump) || input::isKeyReleased(input::Buttons::shoot))
			 && currentDialog.hasFinishedDialog)
		{
			if (!currentDialog.updateDialog())
			{
				currentDialog.close();

				//todo check if bird is in dialog
				if (bird.showing)
				{
					bird.startEndMove(bird.position, getDiagonalBirdPos(bird.position, player.pos));
				}

				saveState( playerSpawnPos, currentLevel, mapData.dialogs, blueChanged, redChanged, grayChanged
				, litTorchesThisGame, litTorchesThisGameCount);
			}

		}
		else if ((input::isKeyReleased(input::Buttons::jump) || input::isKeyReleased(input::Buttons::shoot))
			&& currentDialog.mState == 0 && currentDialog.showing)
		{
			currentDialog.hasFinishedDialog = 1;
			currentDialog.mTextToShow = currentDialog.text;
			currentDialog.text = "";
		}
	}

#pragma endregion

	{

		bool shouldClose = 0;

		if (inGameMenu)
		{

			if (ingameMenuMainPage)
			{

				menu::startMenu(1);

				menu::uninteractableCentreText("Menu");
				bool s = 0;
				bool exit = 0;
				bool controllsButton = 0;

				menu::interactableText("Settings", &s);
				menu::interactableText("Controlls", &controllsButton);
				menu::interactableText("Exit level", &exit);

				bool back = 0;
				menu::endMenu(renderer2d, uiDialogBox, uiBackArrow, font, &back, copyDeltaTime);

				if (back)
				{
					inGameMenu = false;
					shouldClose = true;
				}
				if (s)
				{
					ingameMenuMainPage = 0;
					settings::setMainSettingsPage();
				}
				if (exit)
				{
					saveState(playerSpawnPos, currentLevel, mapData.dialogs, blueChanged, redChanged, grayChanged
						, litTorchesThisGame, litTorchesThisGameCount);

					mapData.cleanup();
					currentLevel = -2;
					loadLevel(); // todo other function for close level
				}
				if (controllsButton) 
				{
					ingameMenuMainPage = 0;
					inGameControllsMenu = 1;
				}
				

			}
			else
			{
				if (inGameControllsMenu) 
				{
					if (input::viewControllsPage(renderer2d, uiDialogBox, uiBackArrow, deltaTime))
					{
						ingameMenuMainPage = 1;
						inGameControllsMenu = 0;
					}
				}
				else
				{
					settings::displaySettings(renderer2d, copyDeltaTime);
					if (currentSettingsMenu == 0)
					{
						ingameMenuMainPage = 1;
						inGameControllsMenu = 0;

					};
				}

				
			}

			if (
				input::isControllerInput()
				&& input::isKeyReleased(input::Buttons::menu)
				)
			{
				//todo fix somehow
				inGameMenu = false;
				shouldClose = true;
			}

			soundManager.updateSoundVolume();

		}

		if (input::isKeyReleased(input::Buttons::menu) && !shouldClose)
		{
			inGameMenu = true;
			ingameMenuMainPage = 1;
			settings::setMainSettingsPage();
		}


	}

	renderer2d.flush();

	return true;

}

void closeGame()
{
	settings::saveSettings();

	if(currentLevel >= 0)
	{
		saveState(playerSpawnPos, currentLevel, mapData.dialogs, blueChanged, redChanged, grayChanged
		,litTorchesThisGame ,litTorchesThisGameCount);
	}

	soundManager.stoppMusic();

}

void imguiFunc(float deltaTime)
{

#ifndef RemoveImgui

	static bool active = 0;
	static glm::vec4 color;

	//todo delta time
	/*
	extern float gravitationalAcceleration;
extern float jumpSpeed;
extern float jumpFromWallSpeed;
extern float velocityClamp;
//extern float drag;
//extern float strafeSpeed = 10;
extern float runSpeed;
extern float airRunSpeed;
extern float grabMargin;
extern float notGrabTimeVal;
extern bool snapWallGrab;
	*/

	//ImGui::Begin("delta");
	//ImGui::Text(std::to_string(1.f/(deltaTime/1000.f)).c_str());
	//ImGui::End();

	//ImGui::Begin("image");
	//ImGui::Image((void*)(intptr_t)characterSprite.id,
	//	{ 4 * 60, 8*60 });
	//ImGui::End();

	//ImGui::Begin("Move settings");
	//ImGui::SliderFloat("gravitationalAcceleration", &gravitationalAcceleration, 30, 100);
	//ImGui::SliderFloat("jumpSpeed", &jumpSpeed, 1, 50);
	//ImGui::SliderFloat("jumpFromWallSpeed", &jumpFromWallSpeed, 1, 50);
	//ImGui::SliderFloat("velocityClamp", &velocityClamp, 10, 70);
	//ImGui::SliderFloat("runSpeed", &runSpeed, 1, 40);
	//ImGui::SliderFloat("airRunSpeed", &airRunSpeed, 1, 40);
	//ImGui::SliderFloat("grabMargin", &grabMargin, 0, 1);
	//ImGui::Checkbox("snapWallGrab", &snapWallGrab);
	//
	//gl2d::TextureAtlas spriteAtlas(BLOCK_COUNT, 4);
	//
	//int mCount = 0;
	//while(mCount < (int)Block::lastBlock)
	//{
	//	
	//		ImGui::Image((void*)(intptr_t)sprites.id,
	//			{ 120,120 }, { spriteAtlas.get(mCount, 0).x, spriteAtlas.get(mCount,0).y }, { spriteAtlas.get(mCount, 0).z, spriteAtlas.get(mCount,0).w }, { 1,1,1,1 }, { 1,0,0,1 });
	//		
	//	if(mCount %5 != 0)
	//	ImGui::SameLine();
	//	
	//	mCount++;
	//}
	//	
	//ImGui::End();
	//
	//ImGui::Begin("My First Tool", &active, ImGuiWindowFlags_MenuBar);
	//if (ImGui::BeginMenuBar())
	//{
	//	if (ImGui::BeginMenu("File"))
	//	{
	//		if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
	//		if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
	//		if (ImGui::MenuItem("Close", "Ctrl+W")) { active = false; }
	//		ImGui::EndMenu();
	//	}
	//	ImGui::EndMenuBar();
	//}
	//
	//// Edit a color (stored as ~4 floats)
	//ImGui::ColorEdit4("Color", &color[0]);
	//
	//// Plot some values
	//const float my_values[] = { 0.2f, 0.1f, 1.0f, 0.5f, 0.9f, 2.2f };
	//ImGui::PlotLines("Frame Times", my_values, IM_ARRAYSIZE(my_values));
	//
	//// Display contents in a scrolling region
	//ImGui::TextColored(ImVec4(1, 1, 0, 1), "Important Stuff");
	//ImGui::BeginChild("Scrolling");
	//for (int n = 0; n < 50; n++)
	//	ImGui::Text("%04d: Some text", n);
	//ImGui::EndChild();
	//ImGui::End();
#endif 


}
