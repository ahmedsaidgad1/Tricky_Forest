#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <algorithm>

using namespace sf;
using namespace std;

//states
enum PlayerState { IDLE, WALK, JUMP };
enum BoxType { GROUND, RECT_HAZARD, ELLIPSE_HAZARD, DOOR, BACK_DOOR, KEY, GATE, BOUNCE };
enum GameState { MENU, LEVEL_SELECT, PLAYING, LEVEL_COMPLETED, PAUSED, CUTSCENE, DEAD };
enum EnemyBehavior { STATIC_WAIT, PATROL, CHASE_ALWAYS, CIRCULAR_PATROL };

struct Hitbox {
    float x, y, w, h;
    BoxType type;
};

struct GuiButton {
    Sprite sprite;
    bool isClicked(RenderWindow& window) {
        if (sprite.getGlobalBounds().contains(Vector2f(Mouse::getPosition(window)))) {
            return Mouse::isButtonPressed(Mouse::Left);
        }
        return false;
    }
};

struct Player {
    Texture texture;
    Sprite sprite;
    PlayerState currentState = IDLE;
    float velocityX = 300, velocityY = 0;
    float gravity = 1200.0f;
    float jumpForce = -800.0f;
    bool isGrounded = false;
    bool hasKey = false;
    float frameTimer = 0;
    int currentFrame = 0;
    const int frameWidth = 154;
    const int frameHeight = 279;

    void setState(PlayerState newState) {
        if (currentState != newState) {
            currentState = newState;
            currentFrame = 0;
            frameTimer = 0;
        }
    }

    void updateAnimation(float dt) {
        frameTimer += dt;
        int row; int maxFrames; float anispeed;
        if (currentState == IDLE) { row = 0; maxFrames = 20; anispeed = 0.1; }
        else if (currentState == WALK) { row = 2; maxFrames = 20; anispeed = 0.05; }
        else if (currentState == JUMP) { row = 1; maxFrames = 8; anispeed = 0.12; }

        if (frameTimer >= anispeed) {
            currentFrame = (currentFrame + 1) % maxFrames;
            frameTimer = 0;
        }
        sprite.setTextureRect(IntRect(currentFrame * frameWidth, row * frameHeight, frameWidth, frameHeight));
    }
};

struct FlyingEnemy {
    Sprite sprite;
    float speed = 150.0f;
    float angle = 0.0f;
    Vector2f center;
    float radius = 200.0f;
    float angularSpeed = 1.5f;
    float frameTimer = 0.0f;
    int currentFrame = 0;
    float scale = 0.4f;
    const int frameWidth = 100;
    const int frameHeight = 70;

    EnemyBehavior behavior = CHASE_ALWAYS;

    Vector2f pointA, pointB;

    bool movingToB = true;

    bool isTriggered = false;

    void update(Vector2f playerPos, float dt) {
        Vector2f currentPos = sprite.getPosition();
        Vector2f direction(0, 0);

        if (behavior == CHASE_ALWAYS || (behavior == STATIC_WAIT && isTriggered)) {
            direction = playerPos - currentPos;
        }
        else if (behavior == PATROL) {
            Vector2f target = movingToB ? pointB : pointA;
            direction = target - currentPos;

            float distToTarget = sqrt(direction.x * direction.x + direction.y * direction.y);
            if (distToTarget < 5.0f) {
                movingToB = !movingToB;
            }
        }
        else if (behavior == CIRCULAR_PATROL) {
            angle += angularSpeed * dt;
            Vector2f newPos(
                center.x + radius * cos(angle),
                center.y + radius * sin(angle)
            );
            Vector2f dir = newPos - sprite.getPosition();
            sprite.setPosition(newPos);

            if (dir.x > 0) sprite.setScale(-scale, scale);
            else sprite.setScale(scale, scale);


              
              
            return;
        }

        float distance = sqrt(direction.x * direction.x + direction.y * direction.y);
        if (distance > 0) {
            direction /= distance;
            sprite.move(direction * speed * dt);

            if (direction.x > 0) sprite.setScale(-1, 1);
            else sprite.setScale(1, 1);
        }
    }
};
struct FinalBoss {
    Sprite sprite;
    Texture* texture;

    
    vector<Vector2f> waypoints;
    int currentWaypoint = 0;
    float speed = 300.0f;

    
    int currentFrame = 0;
    float frameTimer = 0.0f;
    int frameWidth = 100;   
    int frameHeight = 70; 
    int totalFrames = 4;  
    float animSpeed = 0.1f;

    // Death
    bool isDying = false;
    bool isDead = false;
    int deathFrame = 0;
    float deathTimer = 0.0f;
    int totalDeathFrames = 6;
    float deathAnimSpeed = 0.12f;

    void update(float dt) {
        if (isDead) return;

        if (isDying) {
            deathTimer += dt;
            if (deathTimer >= deathAnimSpeed) {
                deathFrame++;
                deathTimer = 0;
                if (deathFrame >= totalDeathFrames) {
                    isDead = true;
                }
            }
            
            sprite.setTextureRect(IntRect(deathFrame * frameWidth, 1 * frameHeight, frameWidth, frameHeight));
            return;
        }

        
        Vector2f target = waypoints[currentWaypoint];
        Vector2f dir = target - sprite.getPosition();
        float dist = sqrt(dir.x * dir.x + dir.y * dir.y);

        if (dist < 5.0f) {
            
            currentWaypoint = (currentWaypoint + 1) % waypoints.size();
        }
        else {
            dir /= dist;
            sprite.move(dir * speed * dt);
            
            if (dir.x > 0) sprite.setScale(-6.0f, 6.0f);
            else sprite.setScale(6.0f, 6.0f);
        }

        // Walk animation (row 0)
        frameTimer += dt;
        if (frameTimer >= animSpeed) {
            currentFrame = (currentFrame + 1) % totalFrames;
            frameTimer = 0;
        }
        sprite.setTextureRect(IntRect(currentFrame * frameWidth, 0, frameWidth, frameHeight));
    }
};
struct Fire {
    Sprite sprite;
    bool isActive = false;
    int currentFrame = 0;
    float frameTimer = 0.0f;
    int frameWidth = 100; 
    int frameHeight = 71;
    int totalFrames = 5;    
    float animSpeed = 0.08f;
    float deathTimer = 0.0f;
    Vector2f position;

    void update(float dt) {
        if (!isActive) return;
        frameTimer += dt;
        if (frameTimer >= animSpeed) {
            currentFrame = (currentFrame + 1) % totalFrames;
            frameTimer = 0;
        }
        sprite.setTextureRect(IntRect(currentFrame * frameWidth, 0, frameWidth, frameHeight));
    }
};
struct Switch {
    Sprite sprite;
    bool isOn = false;
    int id;

    void toggle() {
        isOn = !isOn;
        sprite.setScale(isOn ? -0.4f : 0.4f, 0.4f);
    }
};

struct Room {
    vector<Hitbox> boxes;
    vector<FlyingEnemy> enemies;
    vector<Switch> switches;
    Vector2f spawnPoint;
    Vector2f second_spawnPoint;
    shared_ptr<Texture> tex;
    Sprite sprite;

    Room() {
        tex = make_shared<Texture>();
    }
};

struct Level {
    int id;
    bool locked = true;
    vector<Room> rooms;
    vector<int> requiredOrder = { 1, 2, 3, 4 };
    vector<int> playerInputOrder;
    int roomnum;
    int currentRoomIndex = 0;
};

struct LevelButton {
    Sprite sprite;
    int levelId;
};


Room loadRoom(string txtPath, string imgPath, const Texture& enemyTexture, const Texture& switchTexture, const Texture& bossTexture) {
    Room room;

    room.tex->loadFromFile(imgPath);
    room.sprite.setTexture(*room.tex);

    ifstream file(txtPath);
    string line;

    while (getline(file, line)) {
        stringstream ss(line);
        string type;
        ss >> type;

        if (type == "enemy_chase") {
            float x, y;
            ss >> x >> y;
            FlyingEnemy e;
            e.sprite.setTexture(enemyTexture);
            FloatRect bounds = e.sprite.getLocalBounds();
            e.sprite.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
            e.sprite.setPosition(x, y);
            e.behavior = CHASE_ALWAYS;
            room.enemies.push_back(e);
        }
        else if (type == "enemy_patrol") {
            float x1, y1, x2, y2;
            ss >> x1 >> y1 >> x2 >> y2;
            FlyingEnemy e;
            e.sprite.setTexture(enemyTexture);

            FloatRect bounds = e.sprite.getLocalBounds();
            e.sprite.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
            e.sprite.setScale(0.4f, 0.4f);

            e.sprite.setPosition(x1, y1);
            e.pointA = Vector2f(x1, y1);
            e.pointB = Vector2f(x2, y2);
            e.behavior = PATROL;
            room.enemies.push_back(e);
        }

        else if (type == "enemy_static") {
            float x, y;
            ss >> x >> y;
            FlyingEnemy e;
            e.sprite.setTexture(enemyTexture);
            FloatRect bounds = e.sprite.getLocalBounds();
            e.sprite.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
            e.sprite.setPosition(x, y);
            e.behavior = STATIC_WAIT;
            e.isTriggered = false;
            room.enemies.push_back(e);
        }
        else if (type == "enemy_circular") {
            float cx, cy, radius, angSpeed;
            ss >> cx >> cy >> radius >> angSpeed;

            FlyingEnemy e;
            e.sprite.setTexture(bossTexture);
            FloatRect bounds = e.sprite.getLocalBounds();
            e.sprite.setOrigin(bounds.width / 2.0f, bounds.height / 2.0f);
            e.scale = 1.0f;                                
            e.sprite.setScale(e.scale, e.scale);          
            e.behavior = CIRCULAR_PATROL;
            e.center = Vector2f(cx, cy);
            e.radius = radius;
            e.angularSpeed = angSpeed;
            e.angle = 3.14159f / 4.0f;
            e.sprite.setPosition(cx + radius * cos(e.angle), cy + radius * sin(e.angle));

            room.enemies.push_back(e);
        }

        else if (type == "spawn") {
            ss >> room.spawnPoint.x >> room.spawnPoint.y;
        }
        else if (type == "second_spawn") {
            ss >> room.second_spawnPoint.x >> room.second_spawnPoint.y;
        }

        else if (type == "switch") {
            float x, y;
            int id;
            ss >> x >> y >> id;
            Switch s;
            s.sprite.setTexture(switchTexture);
            s.sprite.setOrigin(s.sprite.getLocalBounds().width / 2.0f, s.sprite.getLocalBounds().height / 2.0f);
            s.sprite.setPosition(x, y);
            s.sprite.setScale(0.4f, 0.4f);
            s.id = id;
            room.switches.push_back(s);
        }

        else {
            float x, y, w, h;
            ss >> x >> y >> w >> h;

            BoxType boxType;

            if (type == "ground") boxType = GROUND;
            else if (type == "door") boxType = DOOR;
            else if (type == "back_door") boxType = BACK_DOOR;
            else if (type == "hazard_rect") boxType = RECT_HAZARD;
            else if (type == "hazard_ellipse") boxType = ELLIPSE_HAZARD;
            else if (type == "key") boxType = KEY;
            else if (type == "gate") boxType = GATE;
            else if (type == "bounce") boxType = BOUNCE;
            else continue;

            room.boxes.push_back({ x, y, w, h, boxType });
        }
    }

    return room;
}

vector<Room> loadAllRooms(string levelPath, const Texture& enemyTexture, const Texture& switchTexture, const Texture& bossTexture) {

    vector<pair<int, Room>> tempRooms;

    for (const auto& entry : filesystem::directory_iterator(levelPath)) {

        if (entry.path().extension() == ".txt") {

            string txtPath = entry.path().string();

            string filename = entry.path().stem().string();
            int roomNumber = stoi(filename.substr(4));

            string imgPath = txtPath;
            imgPath.replace(imgPath.find(".txt"), 4, ".png");

            Room room = loadRoom(txtPath, imgPath, enemyTexture, switchTexture, bossTexture);
            tempRooms.push_back({ roomNumber, room });
        }
    }

    sort(tempRooms.begin(), tempRooms.end(),
        [](const pair<int, Room>& a, const pair<int, Room>& b) {
            return a.first < b.first;
        });

    vector<Room> rooms;
    for (auto& p : tempRooms) {
        rooms.push_back(p.second);
    }

    return rooms;
}

void saveProgress(int maxUnlocked) {
    ofstream file("assets/save.txt");
    if (file.is_open()) {
        file << maxUnlocked;
        file.close();
    }
}

int loadProgress() {
    int maxUnlocked = 1;
    ifstream file("assets/save.txt");
    if (file.is_open()) {
        file >> maxUnlocked;
        file.close();
    }
    return maxUnlocked;
}

Music backgroundMusic;

void playBackgroundMusic(const string& filename) {
    static string currentFile = "";
    if (currentFile == filename && backgroundMusic.getStatus() == Music::Playing) return;

    backgroundMusic.stop();
    if (backgroundMusic.openFromFile(filename)) {
        currentFile = filename;
        backgroundMusic.setLoop(true);
        backgroundMusic.play();
    }
}
void resetLevel(Level& lvl, Player& p, const Texture& enemyTex, const Texture& switchTexture, const Texture& bossTexture, bool& l3_key) {
    string folderPath = "levels/level-" + to_string(lvl.id);
    lvl.rooms = loadAllRooms(folderPath, enemyTex, switchTexture, bossTexture);
    lvl.currentRoomIndex = 0;
    lvl.playerInputOrder.clear();

    p.sprite.setPosition(lvl.rooms[0].spawnPoint);
    p.velocityY = 0;
    p.velocityX = 300.0f;
    p.hasKey = false;
    l3_key = false;
}


int main() {
    RenderWindow window(VideoMode(1920, 1080), "Tricky Forest");
    Clock game_clock;

    GameState currentState = MENU;
    GameState lastState = PAUSED;

    // ---------- MENU TEXTURES ----------
    vector<LevelButton> levelButtons;
    Texture playTex, exitTex, leveltex, lockTex, hometex, nexttex, repeartex;
    playTex.loadFromFile("assets/buttons/play_btn.png");
    exitTex.loadFromFile("assets/buttons/Exit_btn.png");
    lockTex.loadFromFile("levels/Locked.png");
    leveltex.loadFromFile("levels/Dummy.png");
    hometex.loadFromFile("assets/buttons/Home.png");
    nexttex.loadFromFile("assets/buttons/Next.png");
    repeartex.loadFromFile("assets/buttons/Repeat.png");

    Texture background, levelbg;
    Sprite menu_bg, level_bg;
    background.loadFromFile("assets/menu bg.png");
    levelbg.loadFromFile("levels/level_select.png");
    menu_bg.setTexture(background);
    level_bg.setTexture(levelbg);
    Texture switchTexture;
    switchTexture.loadFromFile("assets/static/switch.png");
    Texture bossTex, finalBossTex, fireTex;
    bossTex.loadFromFile("assets/characters/boss.png");
    finalBossTex.loadFromFile("assets/characters/final_boss.png");
    fireTex.loadFromFile("assets/characters/fire.png");

    FinalBoss finalBoss;
    finalBoss.texture = &finalBossTex;
    finalBoss.sprite.setTexture(finalBossTex);
    finalBoss.sprite.setOrigin(finalBoss.frameWidth / 2.0f, finalBoss.frameHeight / 2.0f);
    finalBoss.sprite.setScale(2.0f, 2.0f);
    finalBoss.sprite.setPosition(2452, 1843); // starting position
    finalBoss.waypoints = {
    {338, 1751},
    {354, 350},
    {3393, 350}, 
    {3350, 1843}
    };
    Fire fire;
    fire.sprite.setTexture(fireTex);
    fire.sprite.setOrigin(fire.frameWidth / 2.0f, fire.frameHeight / 2.0f);
    fire.sprite.setScale(6.0f,6.0f);
    fire.position = { 2867, 1955 };
    fire.sprite.setPosition(fire.position);

    // ---------- HINT SYSTEM ----------
    Texture hintTex[5];
    Sprite  hintSprite[5];
    for (int i = 1; i <= 4; i++) {
        hintTex[i].loadFromFile("levels/hints/hint_level" + to_string(i + 1) + ".png");
        hintSprite[i].setTexture(hintTex[i]);
        hintSprite[i].setPosition(
            (1920 - hintTex[i].getSize().x) / 2.0f,
            (1080 - hintTex[i].getSize().y) / 2.0f
        );
    }
    FloatRect hintArea(30, 30, 100, 100);
    bool showHint = false;

    int totalLevels = 12;
    Font font;
    font.loadFromFile("assets/Rubik-Bold.ttf");
    Text levelnum;
    levelnum.setFont(font);
    levelnum.setFillColor(Color::Black);
    levelnum.setCharacterSize(45);

    for (int i = 0; i < totalLevels; i++) {
        LevelButton b;
        b.levelId = i + 1;

        b.sprite.setTexture(leveltex);

        int cols = 4;
        float startX = 300;
        float startY = 200;
        float spacingX = 400;
        float spacingY = 250;

        int row = i / cols;
        int col = i % cols;

        b.sprite.setPosition(startX + col * spacingX, startY + row * spacingY);

        levelButtons.push_back(b);
    }

    GuiButton playBtn, exitBtn, homebtn, nextbtn, repeatbtn;
    playBtn.sprite.setTexture(playTex);
    exitBtn.sprite.setTexture(exitTex);
    homebtn.sprite.setTexture(hometex);
    nextbtn.sprite.setTexture(nexttex);
    repeatbtn.sprite.setTexture(repeartex);
    playBtn.sprite.setPosition(200, 300);
    exitBtn.sprite.setPosition(200, 680);
    homebtn.sprite.setPosition(200, 950);

    // --- Audio Setup ---
    SoundBuffer jumpBuffer, deathBuffer, keyBuffer, walkBuffer, gateBuffer;
    jumpBuffer.loadFromFile("assets/sounds/jumpkemo.mp3");
    deathBuffer.loadFromFile("death (1).mp3");
    keyBuffer.loadFromFile("assets/sounds/keycollect.mp3");
    walkBuffer.loadFromFile("walk2.mp3");
    gateBuffer.loadFromFile("gate1.mp3");

    Sound jumpSound(jumpBuffer);
    jumpSound.setVolume(100.0f);
    Sound deathSound(deathBuffer);
    deathSound.setVolume(100.0f);
    Sound walkSound(walkBuffer);
    walkSound.setVolume(100.0f);
    Sound pickupSound(keyBuffer);
    pickupSound.setVolume(100.0f);
    Sound gateSound(gateBuffer);
    gateSound.setVolume(100.0f);

    // ---------- LEVELS ----------

    int maxUnlocked = loadProgress();
    vector<Level> levels;
    int currentLevelIndex = 0;

    Texture enemyTex;
    enemyTex.loadFromFile("assets/characters/enemy.png");
    for (int i = 1; i <= 12; i++) {
        string folderPath = "levels/level-" + to_string(i);

        if (filesystem::exists(folderPath)) {
            Level lvl;
            lvl.id = i;
            lvl.locked = (i > 1);
            lvl.rooms = loadAllRooms(folderPath, enemyTex, switchTexture, bossTex);
            lvl.roomnum = lvl.rooms.size();
            lvl.currentRoomIndex = 0;
            levels.push_back(lvl);
        }
    }

    Player p;
    p.texture.loadFromFile("assets/characters/player sprite.png");
    p.sprite.setTexture(p.texture);
    p.sprite.setOrigin(p.frameWidth / 2.0f, p.frameHeight / 2.0f);
    p.sprite.setScale(0.3f, 0.3f);
    if (!levels[currentLevelIndex].rooms.empty()) {
        p.sprite.setPosition(levels[currentLevelIndex].rooms[0].spawnPoint);
    }

    Texture keyTex, gateTex, bounceTex;
    keyTex.loadFromFile("assets/static/key sprite.png");
    gateTex.loadFromFile("assets/static/gate.png");
    bounceTex.loadFromFile("assets/static/bounce.png");
    Sprite keySprite, gateSprite, bounceSprite;
    keySprite.setTexture(keyTex);
    gateSprite.setTexture(gateTex);
    bounceSprite.setTexture(bounceTex);

    // ---------- CAMERA ----------
    View camera(FloatRect(0, 0, 1920, 1080));


    bool level3_keyVisible = false;

    while (window.isOpen()) {
        float dt = game_clock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;

        if (currentState != lastState) {
            if (currentState == MENU || currentState == LEVEL_SELECT) {
                playBackgroundMusic("assets/sounds/Menu.mp3");
            }
            else if (currentState == PLAYING) {
                if (levels[currentLevelIndex].id == 6) {
                    playBackgroundMusic("assets/sounds/Boss.mp3");
                }
                else {
                    playBackgroundMusic("assets/sounds/Menu.mp3");
                }
            }
            else if (currentState == DEAD) {
                backgroundMusic.stop();
            }

            lastState = currentState;
        }

        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) window.close();
            if (currentState == PLAYING) {
                if ((event.type == Event::KeyPressed && (event.key.code == Keyboard::Space || event.key.code == Keyboard::Up)) && p.isGrounded) {
                    p.velocityY = p.jumpForce;
                    p.isGrounded = false;
                    jumpSound.play();
                }
                if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                    if (showHint && !hintArea.contains(Vector2f(event.mouseButton.x, event.mouseButton.y))) {
                        showHint = false;
                    }
                }
            }
            if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
                if (currentState == PLAYING) { currentState = PAUSED; showHint = false; }
                else if (currentState == PAUSED) currentState = PLAYING;
            }

            if (event.type == Event::KeyPressed && event.key.code == Keyboard::E) {
                Level& currentLevel = levels[currentLevelIndex];
                Room& curRoom = currentLevel.rooms[currentLevel.currentRoomIndex];

                for (auto& s : curRoom.switches) {
                    FloatRect playerBounds = p.sprite.getGlobalBounds();
                    FloatRect switchBounds = s.sprite.getGlobalBounds();
                    switchBounds.left -= 50; switchBounds.width += 50;

                    if (playerBounds.intersects(switchBounds)) {
                        s.toggle();
                        pickupSound.play();

                        if (currentLevel.id == 5) {
                            currentLevel.playerInputOrder.push_back(s.id);

                            bool correctSoFar = true;
                            for (int i = 0; i < currentLevel.playerInputOrder.size(); i++) {
                                if (currentLevel.playerInputOrder[i] != currentLevel.requiredOrder[i]) {
                                    correctSoFar = false;
                                    break;
                                }
                            }

                            if (!correctSoFar) {
                                currentLevel.playerInputOrder.clear();
                                for (auto& sw : curRoom.switches) sw.isOn = false;
                            }
                            else if (currentLevel.playerInputOrder == currentLevel.requiredOrder) {
                                for (int i = 0; i < curRoom.boxes.size(); i++) {
                                    if (curRoom.boxes[i].type == GATE) {
                                        curRoom.boxes.erase(curRoom.boxes.begin() + i);
                                        gateSound.play();
                                        break;
                                    }
                                }
                            }

                            if (!correctSoFar) {
                                currentLevel.playerInputOrder.clear();
                                for (auto& sw : curRoom.switches) {
                                    sw.isOn = false;
                                    sw.sprite.setScale(0.4f, 0.4f); // إرجاع اتجاه المفتاح للوضع الأصلي
                                }
                            }
                        }
                    }
                }
            }
        }

        window.clear();

        // ================= MENU =================

        if (currentState == MENU) {
            window.setView(window.getDefaultView());
            window.draw(menu_bg);
            window.draw(playBtn.sprite);
            window.draw(exitBtn.sprite);
            if (playBtn.isClicked(window)) currentState = LEVEL_SELECT;
            if (exitBtn.isClicked(window)) window.close();
        }

        // ================= LEVEL SELECT =================

        else if (currentState == LEVEL_SELECT) {
            window.setView(window.getDefaultView());
            window.draw(level_bg);
            window.draw(homebtn.sprite);
            if (homebtn.isClicked(window)) currentState = MENU;
            for (auto& b : levelButtons) {

                if (b.levelId > maxUnlocked) {
                    b.sprite.setTexture(lockTex);
                }
                else {
                    b.sprite.setTexture(leveltex);
                }

                window.draw(b.sprite);

                if (b.levelId <= maxUnlocked) {
                    levelnum.setString(to_string(b.levelId));
                    FloatRect textBounds = levelnum.getLocalBounds();
                    levelnum.setOrigin((textBounds.left + textBounds.width / 2.0f) + 5, (textBounds.top + textBounds.height / 2.0f) - 15);
                    levelnum.setPosition(
                        b.sprite.getPosition().x + b.sprite.getGlobalBounds().width / 2.0f,
                        b.sprite.getPosition().y + b.sprite.getGlobalBounds().height / 2.0f);
                    window.draw(levelnum);
                }

                // click
                if (Mouse::isButtonPressed(Mouse::Left) &&
                    b.sprite.getGlobalBounds().contains((Vector2f)Mouse::getPosition(window))) {
                    if (b.levelId <= maxUnlocked) {
                        currentLevelIndex = b.levelId - 1;
                        Level& currentLevel = levels[currentLevelIndex];
                        currentLevel.currentRoomIndex = 0;
                        p.sprite.setPosition(currentLevel.rooms[0].spawnPoint);
                        resetLevel(levels[currentLevelIndex], p, enemyTex, switchTexture, bossTex, level3_keyVisible);
                        showHint = false;
                        currentState = PLAYING;
                    }
                }
            }
        }

        // ================= PLAYING =================

        else if (currentState == PLAYING) {

            Level& currentLevel = levels[currentLevelIndex];
            Room& curRoom = currentLevel.rooms[currentLevel.currentRoomIndex];
            bool moving = false;

            if (Keyboard::isKeyPressed(Keyboard::D) || Keyboard::isKeyPressed(Keyboard::Right)) {
                p.sprite.setScale(0.3f, 0.3f);
                p.sprite.move(p.velocityX * dt, 0);
                moving = true;
            }

            else if (Keyboard::isKeyPressed(Keyboard::A) || Keyboard::isKeyPressed(Keyboard::Left)) {
                p.sprite.setScale(-0.3f, 0.3f);
                p.sprite.move(-p.velocityX * dt, 0);
                moving = true;
            }


            if (moving && p.isGrounded) {
                if (walkSound.getStatus() != Sound::Playing) {
                    walkSound.play();
                }
            }
            else {
                if (walkSound.getStatus() == Sound::Playing) {
                    walkSound.stop();
                }
            }

            if (currentLevel.id == 4) {
                if (Mouse::isButtonPressed(Mouse::Left)) {
                    Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));

                    if (mousePos.x >= 843 && mousePos.x <= 894 &&
                        mousePos.y >= 267 && mousePos.y <= 295) {

                        for (int i = 0; i < curRoom.boxes.size(); i++) {
                            if (curRoom.boxes[i].type == GATE) {
                                curRoom.boxes.erase(curRoom.boxes.begin() + i);
                                gateSound.play();
                                break;
                            }
                        }
                    }
                }
            }

            for (auto& enemy : curRoom.enemies) {
                enemy.update(p.sprite.getPosition(), dt);

                if (p.sprite.getGlobalBounds().intersects(enemy.sprite.getGlobalBounds())) {
                    p.sprite.setPosition(curRoom.spawnPoint);
                    p.velocityY = 0;
                    deathSound.play();
                    currentState = DEAD;
                }

            }
            // --- FINAL BOSS ---
            if (currentLevel.id == 6) {

                for (auto& s : curRoom.switches) {
                    if (s.isOn) fire.isActive = true;
                }

                fire.update(dt);

                if (!finalBoss.isDead) {
                    finalBoss.update(dt);

                    if (fire.isActive && !finalBoss.isDying) {
                        FloatRect fireBounds(
                            fire.sprite.getPosition().x - 30,
                            fire.sprite.getPosition().y - 50,
                            60,
                            100
                        );
                        if (finalBoss.sprite.getGlobalBounds().intersects(fireBounds)) {
                            finalBoss.isDying = true;
                        }
                    }

                    if (!finalBoss.isDying &&
                        p.sprite.getGlobalBounds().intersects(finalBoss.sprite.getGlobalBounds())) {
                        p.sprite.setPosition(curRoom.spawnPoint);
                        p.velocityY = 0;
                        deathSound.play();
                        currentState = DEAD;
                    }
                }

                if (finalBoss.isDead && fire.isActive) {
                    fire.deathTimer += dt;
                    if (fire.deathTimer >= 1.0f) {
                        fire.isActive = false;
                    }
                }

                if (finalBoss.isDead) {
                    for (int i = 0; i < curRoom.boxes.size(); i++) {
                        if (curRoom.boxes[i].type == GATE) {
                            curRoom.boxes.erase(curRoom.boxes.begin() + i);
                            gateSound.play();
                            break;
                        }
                    }
                }
            }
            for (const auto& box : curRoom.boxes) {
                if (box.type != GROUND) continue;

                FloatRect pBounds = p.sprite.getGlobalBounds();
                FloatRect b(box.x, box.y, box.w, box.h);
                FloatRect intersection;

                if (pBounds.intersects(b, intersection)) {
                    if (intersection.width < intersection.height) {
                        if (pBounds.left < b.left)
                            p.sprite.setPosition(b.left - pBounds.width / 2.0f, p.sprite.getPosition().y);
                        else
                            p.sprite.setPosition(b.left + b.width + pBounds.width / 2.0f, p.sprite.getPosition().y);
                    }
                }
            }
            p.velocityY += p.gravity * dt;
            p.sprite.move(0, p.velocityY * dt);
            p.isGrounded = false;
            p.velocityX = 300.0f;
            for (int i = 0; i < curRoom.boxes.size(); i++) {
                auto& box = curRoom.boxes[i];
                FloatRect pBounds = p.sprite.getGlobalBounds();
                FloatRect b(box.x, box.y, box.w, box.h);
                FloatRect intersection;

                if (pBounds.intersects(b, intersection)) {
                    if (box.type == KEY && !p.hasKey) {
                        if (Keyboard::isKeyPressed(Keyboard::E)) {
                            p.hasKey = true;
                            pickupSound.play();
                            curRoom.boxes.erase(curRoom.boxes.begin() + i);
                            i--; continue;
                        }
                    }

                    if (box.type == GATE) {

                        if (currentLevel.id == 2) {
                            if (pBounds.intersects(b)) {

                                float pushSpeed = 100.0f;
                                box.x += (p.sprite.getScale().x > 0 ? pushSpeed : -pushSpeed) * dt;
                                p.velocityX = 100.0f;
                            }
                            else {
                                p.velocityX = 300.0f;
                            }
                        }
                        else {
                            if (p.hasKey) {
                                p.hasKey = false;
                                gateSound.play();
                                curRoom.boxes.erase(curRoom.boxes.begin() + i);
                                i--; continue;
                            }
                            else {
                                if (intersection.width < intersection.height) {
                                    if (pBounds.left < b.left) p.sprite.setPosition(b.left - pBounds.width / 2.0f, p.sprite.getPosition().y);
                                    else p.sprite.setPosition(b.left + b.width + pBounds.width / 2.0f, p.sprite.getPosition().y);
                                }
                                continue;
                            }
                        }
                    }

                    // BOUNCE player upward
                    if (box.type == BOUNCE) {
                        FloatRect bounceBounds(
                            box.x + 20,       // left edge (shrink from left)
                            box.y + 80,       // top edge (shrink from top)
                            box.w - 20,       // width (smaller than full box)
                            box.h - 10        // height (smaller than full box)
                        );
                        if (pBounds.intersects(bounceBounds)) {
                            if (p.velocityY > 0 && pBounds.top < bounceBounds.top) {
                                p.sprite.setPosition(p.sprite.getPosition().x, bounceBounds.top - pBounds.height / 2.0f);
                                p.velocityY = -1900.0f;
                                p.isGrounded = false;
                            }
                        }
                        continue;
                    }

                    if (box.type == RECT_HAZARD || box.type == ELLIPSE_HAZARD) {
                        p.sprite.setPosition(curRoom.spawnPoint);
                        p.velocityY = 0;
                        deathSound.play();
                        currentState = DEAD;
                        break;
                    }

                    if (intersection.height <= intersection.width) {
                        if (p.velocityY > 0 && pBounds.top < b.top) {
                            p.sprite.setPosition(p.sprite.getPosition().x, b.top - pBounds.height / 2.0f);
                            p.velocityY = 0; p.isGrounded = true;
                        }
                        else if (p.velocityY < 0 && pBounds.top > b.top) {
                            p.sprite.setPosition(p.sprite.getPosition().x, b.top + b.height + pBounds.height / 2.0f);
                            p.velocityY = 0;
                        }
                    }

                    if (box.type == DOOR) {
                        if (currentLevel.currentRoomIndex + 1 < currentLevel.rooms.size()) {
                            currentLevel.currentRoomIndex++;
                            p.sprite.setPosition(currentLevel.rooms[currentLevel.currentRoomIndex].spawnPoint);
                            p.velocityY = 0;
                            showHint = false;
                            camera.setCenter(currentLevel.rooms[currentLevel.currentRoomIndex].spawnPoint);
                        }
                        else {
                            currentState = LEVEL_COMPLETED;
                            if (currentLevelIndex + 1 < levels.size()) {
                                maxUnlocked = max(maxUnlocked, currentLevelIndex + 2);
                                saveProgress(maxUnlocked);
                            }
                        }
                    }
                    if (box.type == BACK_DOOR) {
                        if (currentLevel.currentRoomIndex > 0) {
                            currentLevel.currentRoomIndex--;
                            Room& previousRoom = currentLevel.rooms[currentLevel.currentRoomIndex];
                            p.sprite.setPosition(previousRoom.second_spawnPoint);
                            camera.setCenter(previousRoom.second_spawnPoint);
                            break;
                            p.velocityY = 0;
                        }
                    }
                }
            }

            if (!p.isGrounded) p.setState(JUMP);
            else if (moving) p.setState(WALK);
            else p.setState(IDLE);

            p.updateAnimation(dt);

            // ---- CAMERA ----
            float roomWidth = (float)curRoom.tex->getSize().x;
            float roomHeight = (float)curRoom.tex->getSize().y;
            Vector2f target = p.sprite.getPosition();
            Vector2f current = camera.getCenter();
            float smoothSpeed = 5.0f;
            camera.setCenter(current + (target - current) * smoothSpeed * dt);
            float halfW = 1920 / 2.0f;
            float halfH = 1080 / 2.0f;
            float camX = camera.getCenter().x;
            float camY = camera.getCenter().y;
            if (camX - halfW < 0)          camX = halfW;
            if (camX + halfW > roomWidth)  camX = roomWidth - halfW;
            if (camY - halfH < 0)          camY = halfH;
            if (camY + halfH > roomHeight) camY = roomHeight - halfH;
            camera.setCenter(camX, camY);
            window.setView(camera);

            window.draw(curRoom.sprite);
            for (auto& enemy : curRoom.enemies) {
                window.draw(enemy.sprite);
            }
            for (auto& s : curRoom.switches) {
                window.draw(s.sprite);
            }
            window.draw(p.sprite);
            if (currentLevel.id == 6) {
                if (fire.isActive) window.draw(fire.sprite);
                if (!finalBoss.isDead) window.draw(finalBoss.sprite);
            }
            for (const auto& box : curRoom.boxes) {
                if (box.type == KEY) {
                    keySprite.setPosition(box.x, box.y);
                    float scaleX = box.w / keySprite.getLocalBounds().width;
                    float scaleY = box.h / keySprite.getLocalBounds().height;
                    keySprite.setScale(scaleX, scaleY);
                    if (box.type == KEY) {
                        if (currentLevel.id == 3 && !level3_keyVisible) continue;
                        window.draw(keySprite);
                    }
                }
                if (box.type == GATE) {
                    gateSprite.setPosition(box.x, box.y);
                    float scaleX = box.w / gateSprite.getLocalBounds().width;
                    float scaleY = box.h / gateSprite.getLocalBounds().height;
                    gateSprite.setScale(scaleX, scaleY);
                    window.draw(gateSprite);
                }
                //  Draw bounce pad sprite
                if (box.type == BOUNCE) {
                    bounceSprite.setPosition(box.x, box.y);
                    float scaleX = box.w / bounceSprite.getLocalBounds().width;
                    float scaleY = box.h / bounceSprite.getLocalBounds().height;
                    bounceSprite.setScale(scaleX, scaleY);
                    window.draw(bounceSprite);
                }
            }
            if (p.hasKey) {
                keySprite.setPosition(p.sprite.getPosition().x + 10, p.sprite.getPosition().y);
                if (p.sprite.getScale().x < 0) {
                    keySprite.setScale(-0.2f, 0.2f);
                    keySprite.setPosition(p.sprite.getPosition().x - 10, p.sprite.getPosition().y);
                }
                else {
                    keySprite.setScale(0.2f, 0.2f);
                }
                window.draw(keySprite);
            }

            // ---- HINT SYSTEM ----
            bool levelHasHint = (currentLevelIndex >= 1 && currentLevelIndex <= 4);
            if (levelHasHint) {
                if (Mouse::isButtonPressed(Mouse::Left) &&
                    hintArea.contains(Vector2f(Mouse::getPosition(window)))) {
                    showHint = true;
                }
                if (showHint) {
                    window.setView(window.getDefaultView());
                    window.draw(hintSprite[currentLevelIndex]);
                    window.setView(camera);
                }
                if (currentLevel.id == 3 && showHint) {
                    if (Mouse::isButtonPressed(Mouse::Left)) {
                        Vector2i mousePos = Mouse::getPosition(window);
                        if (mousePos.x > 970 && mousePos.x < 1150 && mousePos.y > 570 && mousePos.y < 640) {
                            level3_keyVisible = true;
                            pickupSound.play();
                            showHint = false;
                        }
                    }
                }
            }
        }

        // ================= LEVEL COMPLETED =================

        else if (currentState == LEVEL_COMPLETED) {
            window.setView(window.getDefaultView());
            Level& currentLevel = levels[currentLevelIndex];
            Room& curRoom = currentLevel.rooms[currentLevel.currentRoomIndex];
            window.draw(curRoom.sprite);

            RectangleShape overlay(Vector2f(1920, 1080));
            overlay.setFillColor(Color(0, 0, 0, 150));
            window.draw(overlay);
            homebtn.sprite.setPosition(810, 540);
            repeatbtn.sprite.setPosition(910, 540);
            nextbtn.sprite.setPosition(1010, 540);

            window.draw(homebtn.sprite);
            window.draw(repeatbtn.sprite);
            window.draw(nextbtn.sprite);

            if (homebtn.isClicked(window)) {
                currentState = MENU;
                homebtn.sprite.setPosition(200, 950);
            }
            else if (repeatbtn.isClicked(window)) {
                Level& currentLevel = levels[currentLevelIndex];
                currentLevel.currentRoomIndex = 0;
                p.sprite.setPosition(currentLevel.rooms[0].spawnPoint);
                resetLevel(levels[currentLevelIndex], p, enemyTex, switchTexture, bossTex, level3_keyVisible);
                showHint = false;

                finalBoss.isDead = false;
                finalBoss.isDying = false;
                finalBoss.deathFrame = 0;
                finalBoss.currentWaypoint = 0;
                finalBoss.sprite.setPosition(2116, 1775);
                fire.isActive = false;
                fire.currentFrame = 0;
                fire.deathTimer = 0.0f;

                currentState = PLAYING;
            }
            else if (nextbtn.isClicked(window)) {
                if (currentLevelIndex + 1 < levels.size()) {
                    currentLevelIndex++;
                    levels[currentLevelIndex].currentRoomIndex = 0;
                    p.sprite.setPosition(levels[currentLevelIndex].rooms[0].spawnPoint);
                    showHint = false;
                    currentState = PLAYING;
                }
            }
        }

        // ================= PAUSED =================

        else if (currentState == PAUSED) {
            window.setView(camera);
            Level& currentLevel = levels[currentLevelIndex];
            Room& curRoom = currentLevel.rooms[currentLevel.currentRoomIndex];
            window.draw(curRoom.sprite);
            for (auto& enemy : curRoom.enemies) {
                window.draw(enemy.sprite);
            }
            window.draw(p.sprite);

            window.setView(window.getDefaultView());
            RectangleShape overlay(Vector2f(1920, 1080));
            overlay.setFillColor(Color(0, 0, 0, 150));
            window.draw(overlay);
            homebtn.sprite.setPosition(810, 540);
            repeatbtn.sprite.setPosition(910, 540);
            nextbtn.sprite.setPosition(1010, 540);

            window.draw(homebtn.sprite);
            window.draw(repeatbtn.sprite);
            window.draw(nextbtn.sprite);

            if (homebtn.isClicked(window)) {
                currentState = MENU;
                homebtn.sprite.setPosition(200, 950);
            }
            else if (repeatbtn.isClicked(window)) {
                Level& currentLevel = levels[currentLevelIndex];
                currentLevel.currentRoomIndex = 0;
                p.sprite.setPosition(currentLevel.rooms[0].spawnPoint);
                resetLevel(levels[currentLevelIndex], p, enemyTex, switchTexture, bossTex, level3_keyVisible);
                showHint = false;

                finalBoss.isDead = false;
                finalBoss.isDying = false;
                finalBoss.deathFrame = 0;
                finalBoss.currentWaypoint = 0;
                finalBoss.sprite.setPosition(2116, 1775);
                fire.isActive = false;
                fire.currentFrame = 0;
                fire.deathTimer = 0.0f;

                currentState = PLAYING;
            }
            else if (nextbtn.isClicked(window)) {
                currentState = PLAYING;
            }
        }

        // ================= DEAD =================

        else if (currentState == DEAD) {
            window.setView(window.getDefaultView());
            Level& currentLevel = levels[currentLevelIndex];
            Room& curRoom = currentLevel.rooms[currentLevel.currentRoomIndex];
            p.hasKey = false;

            window.draw(curRoom.sprite);

            RectangleShape overlay(Vector2f(1920, 1080));
            overlay.setFillColor(Color(255, 0, 0, 100));
            window.draw(overlay);

            homebtn.sprite.setPosition(860, 540);
            repeatbtn.sprite.setPosition(1000, 540);

            window.draw(homebtn.sprite);
            window.draw(repeatbtn.sprite);

            if (homebtn.isClicked(window)) {
                currentState = MENU;
                homebtn.sprite.setPosition(200, 950);
            }
            else if (repeatbtn.isClicked(window)) {
                resetLevel(levels[currentLevelIndex], p, enemyTex, switchTexture, bossTex, level3_keyVisible);
                p.sprite.setPosition(currentLevel.rooms[0].spawnPoint);
                showHint = false;

                
                finalBoss.isDead = false;
                finalBoss.isDying = false;
                finalBoss.deathFrame = 0;
                finalBoss.currentWaypoint = 0;
                finalBoss.sprite.setPosition(2116, 1775);
                fire.isActive = false;
                fire.currentFrame = 0;
                fire.deathTimer = 0.0f;
                currentState = PLAYING;
            
            }
        }

        window.display();

        cout << p.sprite.getPosition().x << p.sprite.getPosition().y;
    }
    return 0;
}
