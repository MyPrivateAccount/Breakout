#include <iostream>
#include <algorithm>

#include "game.h"
#include "../core/resource_manager.h"
#include "../render/sprite_renderer.h"
#include "../render/particle.h"
#include "../render/post_processor.h"
#include "../game/ball_object.h"
#include "../utils/collision.h"
#include "../utils/rect.h"
#include "../utils/circle.h"
#include "GLFW/glfw3.h"

SpriteRenderer* sprite_renderer;
ParticleGenerator* particle_generator;
PostProcessor* post_processor;

// 挡板大小
const glm::vec2 kPaddleSize(100, 20);
// 挡板速度
const float kPaddleVelocity(500.0f);
GameObject* paddle;

// 球的速度
const glm::vec2 kBallVelocity(100.0f, -350.0f);
// 球的半径
const float kBallRadius = 12.5f;
BallObject *ball;

// shake特效持续时间
float shake_time = 0;

Game::Game(unsigned int width, unsigned int height){
    width_ = width;
    height_ = height;
}

Game::~Game(){
    delete(sprite_renderer);
}

void Game::Init(){
    
    // 前四个参数依次指定了左，右，下，上边界，后两个参数定义了近平面和远平面的距离
    // 即左上角坐标为(0, 0)，右下角坐标为(width_, height_)
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, -1.0f, 1.0f);

    Shader sprite_shader = ResourceManager::LoadShader("resources/shaders/sprite.vs", "resources/shaders/sprite.frag", nullptr, "sprite");
    sprite_shader.Use().SetInteger("image", 0);
    sprite_shader.SetMatrix4("projection", projection);

    Shader particle_shader = ResourceManager::LoadShader("resources/shaders/particle.vs", "resources/shaders/particle.frag", nullptr, "particle");
    particle_shader.Use().SetInteger("sprite", 0);
    particle_shader.SetMatrix4("projection", projection);

    Shader postprocessing_shader = ResourceManager::LoadShader("resources/shaders/post_processing.vs", "resources/shaders/post_processing.frag", nullptr, "postprocessing");

    // 加载纹理
    ResourceManager::LoadTexture("resources/textures/background.jpg", false, "background");
    ResourceManager::LoadTexture("resources/textures/awesomeface.png", true, "ball");
    ResourceManager::LoadTexture("resources/textures/block_solid.png", false, "block_solid");
    ResourceManager::LoadTexture("resources/textures/block.png", false, "block");
    ResourceManager::LoadTexture("resources/textures/paddle.png", true, "paddle");
    ResourceManager::LoadTexture("resources/textures/particle.png", true, "particle");

    ResourceManager::LoadTexture("resources/textures/powerup_speed.png", true, "powerup_speed");
    ResourceManager::LoadTexture("resources/textures/powerup_sticky.png", true, "powerup_sticky");
    ResourceManager::LoadTexture("resources/textures/powerup_increase.png", true, "powerup_increase");
    ResourceManager::LoadTexture("resources/textures/powerup_confuse.png", true, "powerup_confuse");
    ResourceManager::LoadTexture("resources/textures/powerup_chaos.png", true, "powerup_chaos");
    ResourceManager::LoadTexture("resources/textures/powerup_passthrough.png", true, "powerup_passthrough");

    sprite_renderer = new SpriteRenderer(sprite_shader);
    particle_generator = new ParticleGenerator(particle_shader, ResourceManager::GetTexture("particle"), 500);
    post_processor = new PostProcessor(postprocessing_shader, width_, height_);

    // 加载关卡
    GameLevel one;
    one.Load("resources/levels/one.lvl", width_, height_ * 0.5);
    levels_.push_back(one);

    GameLevel two;
    two.Load("resources/levels/two.lvl", width_, height_ * 0.5);
    levels_.push_back(two);

    GameLevel three;
    three.Load("resources/levels/three.lvl", width_, height_ * 0.5);
    levels_.push_back(three);

    GameLevel four;
    four.Load("resources/levels/four.lvl", width_, height_ * 0.5);
    levels_.push_back(four);

    // 设置当前关卡
    level_ = 0;

    // 初始化挡板
    glm::vec2 paddle_pos = glm::vec2(width_ / 2 - kPaddleSize.x / 2, height_ - kPaddleSize.y);
    paddle = new GameObject(paddle_pos, kPaddleSize, ResourceManager::GetTexture("paddle"));

    // 初始化小球
    glm::vec2 ball_pos = paddle_pos + glm::vec2(kPaddleSize.x / 2 - kBallRadius, -kBallRadius * 2);
    ball = new BallObject(ball_pos, kBallRadius, kBallVelocity, ResourceManager::GetTexture("ball"));
}

void Game::Update(float dt){
    // 移动小球
    ball->Move(dt, width_);

    particle_generator->Update(dt, *ball, 2, glm::vec2(ball->radius_ / 2));

    // 碰撞检测
    DoCollisions();

    UpdatePowerUps(dt);

    if(ball->position_.y >= height_){  // 小球接触底部边界，游戏失败
        Reset();
    }

    if(shake_time > 0.0f){
        shake_time -= dt;
        if(shake_time <= 0.0f)
            post_processor->shake_ = false;
    }
}

void Game::ProcessInput(float dt){
    if(state_ == KGameActive){
        float velocity = kPaddleVelocity * dt;
        // 移动挡板
        if(keys_[GLFW_KEY_A]){
            paddle->position_.x -= velocity;
            if(paddle->position_.x < 0)
                paddle->position_.x = 0;
            if(ball->stuck_){
                ball->position_ = paddle->position_ + glm::vec2(kPaddleSize.x / 2 - kBallRadius, -kBallRadius * 2);
            }
        }
            
        if(keys_[GLFW_KEY_D]){
            paddle->position_.x += velocity;
            if(paddle->position_.x > width_ - paddle->size_.x)
                paddle->position_.x = width_ - paddle->size_.x;
            if(ball->stuck_){
                ball->position_ = paddle->position_ + glm::vec2(kPaddleSize.x / 2 - kBallRadius, -kBallRadius * 2);
            }
        }
            
        if(keys_[GLFW_KEY_SPACE])
            ball->stuck_ = false;
    }
}

void Game::Render(){
    if(state_ == KGameActive){

        post_processor->BeginRender();

        // 绘制背景
        Texture2D texture = ResourceManager::GetTexture("background");
        sprite_renderer->DrawSprite(texture, glm::vec2(0, 0), glm::vec2(width_, height_), 0.0f);

        // 绘制关卡
        levels_[level_].Draw(*sprite_renderer);

        // 绘制挡板
        paddle->Draw(*sprite_renderer);

        // 绘制道具
        for(PowerUp& power_up : power_ups_)
            if(!power_up.destroyed_)
                power_up.Draw(*sprite_renderer);

        // 绘制粒子
        particle_generator->Draw();  // 保证粒子绘制在其他物体之前，小球之后

        // 绘制小球
        ball->Draw(*sprite_renderer);

        post_processor->EndRender();

        post_processor->Render(glfwGetTime());
    }
}

// 激活道具效果
void ActivatePowerUp(PowerUp& power_up){
    if(power_up.type_ == "speed"){
        ball->velocity_ *= 1.2;
    }else if(power_up.type_ == "sticky"){
        ball->sticky_ = true;
        paddle->color_ = glm::vec3(1.0f, 0.5f, 1.0f);
    }else if(power_up.type_ == "pass-through"){
        ball->pass_through_ = true;
        ball->color_ = glm::vec3(1.0f, 0.5f, 0.5f);
    }else if(power_up.type_ == "pad-size-increase"){
        paddle->size_.x += 50;
    }else if(power_up.type_ == "confuse"){
        if(!post_processor->chaos_)
            post_processor->confuse_ = true; 
    }else if(power_up.type_ == "chaos"){
        if(!post_processor->confuse_)
            post_processor->chaos_ = true;
    }
}

void Game::DoCollisions(){
    Circle ball_circle(ball->position_, ball->radius_);
    for(GameObject& box : levels_[level_].bricks_){
        if(!box.destroyed_){
            Rect box_rect(box.position_, box.size_);
            CollisionResult result = Collision::Detect(ball_circle, box_rect);
            if(get<0>(result)){
                // 非坚固砖块，销毁
                if(!box.is_solid_){
                    box.destroyed_ = true;
                    SpawnPowerUps(box);
                }else{  
                    // 坚固砖块触发shake特效
                    shake_time = 0.05f;
                    post_processor->shake_ = true;
                }

                // 碰撞处理
                if(!(ball->pass_through_ && !box.is_solid_)){  // 如果有pass_through道具效果，则不再对非坚固物体产生碰撞
                    Direction dir = get<1>(result);
                    glm::vec2 difference = get<2>(result);
                    if(dir == kLeft || dir == kRight){  // 水平方向碰撞
                        ball->velocity_.x = -ball->velocity_.x;  // 反转水平速度

                        // 将小球移出砖块内
                        float penetration = ball->radius_ - abs(difference.x);
                        if(dir == kLeft)  // 在矩形右边碰撞，将球右移
                            ball->position_.x += penetration;  
                        else
                            ball->position_.x -= penetration;
                    }else{
                        ball->velocity_.y = -ball->velocity_.y;  // 反转垂直速度

                        float penetration = ball->radius_ - abs(difference.y);
                        if(dir == kUp)
                            ball->position_.y -= penetration;
                        else
                            ball->position_.y += penetration;
                    }
                }
            }
        }
    }

    Rect paddle_rect(paddle->position_, paddle->size_);

    for(PowerUp& power_up : power_ups_){
        if(!power_up.destroyed_){
            if(power_up.position_.x >= this->height_)
                power_up.destroyed_ = true;
            Rect power_up_rect(power_up.position_, power_up.size_);
            if(Collision::Detect(paddle_rect, power_up_rect)){
                // 挡板碰到道具，激活道具
                ActivatePowerUp(power_up);
                power_up.destroyed_ = true;
                power_up.activated_ = true;
            }
        }
    }

    CollisionResult result = Collision::Detect(ball_circle, paddle_rect);
    if(!ball->stuck_ && get<0>(result)){
        // 检查碰撞到了挡板的哪个位置，并根据位置来改变速度
        // 小球圆心距离挡板中心越远，水平速度越大
        float paddle_center = paddle->position_.x + paddle->size_.x / 2;
        float distance = (ball->position_.x + ball->radius_) - paddle_center;
        float percentage = distance / (paddle->size_.x / 2);

        float strength = 2.0f;
        glm::vec2 old_velocity = ball->velocity_;
        ball->velocity_.x = kBallVelocity.x * percentage * strength;
        // ball->velocity_.y = - ball->velocity_.y;
        ball->velocity_.y = 1 - abs(ball->velocity_.y);
        // 保证小球总的速度与力量是一致的
        ball->velocity_ = glm::normalize(ball->velocity_) * glm::length(old_velocity);

        ball->stuck_ = ball->sticky_;  // 小球和挡板碰撞后，如果有卡住的道具效果，则应用
    }
}

void Game::Reset(){
    // 重置关卡
    for(unsigned int i = 0; i <= level_; i ++)
        levels_[i].Reset();

    // 重置挡板
    paddle->size_ = kPaddleSize;
    paddle->position_ = glm::vec2(width_ / 2 - kPaddleSize.x / 2, height_ - kPaddleSize.y);
    paddle->color_ = glm::vec3(1.0f);

    // 重置小球
    ball->Reset(paddle->position_ + glm::vec2(kPaddleSize.x / 2 - kBallRadius, -kBallRadius * 2), kBallVelocity);

    // 重置特效
    post_processor->chaos_ = false;
    post_processor->confuse_ = false;

    // 重置道具
    power_ups_.clear();
}

bool shouldSpawn(unsigned int chance){
    unsigned int random = rand() % chance;  // 产生一个0到chance - 1的整数
    return random == 0;
}

void Game::SpawnPowerUps(GameObject &block){

    // 正面道具，出现概率 1/75

    if(shouldSpawn(75))
        power_ups_.push_back(PowerUp("speed", glm::vec3(0.5f, 0.5f, 1.0f), 0.0f, block.position_, ResourceManager::GetTexture("powerup_speed")));
    if(shouldSpawn(75))
        power_ups_.push_back(PowerUp("sticky", glm::vec3(1.0f, 0.5f, 1.0f), 20.0f, block.position_, ResourceManager::GetTexture("powerup_sticky")));
    if(shouldSpawn(75))
        power_ups_.push_back(PowerUp("pass-through", glm::vec3(0.5f, 1.0f, 0.5f), 10.0f, block.position_, ResourceManager::GetTexture("powerup_passthrough")));
    if(shouldSpawn(75))
        power_ups_.push_back(PowerUp("pad-size-increase", glm::vec3(1.0f, 0.6f, 0.4f), 0.0f, block.position_, ResourceManager::GetTexture("powerup_increase")));

    // 负面道具，出现概率 1/15

    if(shouldSpawn(15))
        power_ups_.push_back(PowerUp("confuse", glm::vec3(1.0f, 0.3f, 0.3f), 15.0f, block.position_, ResourceManager::GetTexture("powerup_confuse")));
    if(shouldSpawn(15))
        power_ups_.push_back(PowerUp("chaos", glm::vec3(0.9f, 0.25f, 0.25f), 15.0f, block.position_, ResourceManager::GetTexture("powerup_chaos")));
}

// 检查是否有其他道具激活了指定的效果
bool isOtherPowerUpActive(vector<PowerUp>& power_ups, string type){
    for(const PowerUp& power_up : power_ups){
        if(power_up.activated_)
            if(power_up.type_ == type)
                return true;
    }
    return false;
}

void Game::UpdatePowerUps(float dt){
    for(PowerUp& power_up : power_ups_){
        power_up.position_ += power_up.velocity_ * dt;
        if(power_up.activated_){
            power_up.duration_ -= dt;
            if(power_up.duration_ <= 0.0f){
                power_up.activated_ = false;
                // 停用道具效果
                if(power_up.type_ == "sticky"){
                    if(!isOtherPowerUpActive(power_ups_, "sticky")){
                        ball->sticky_ = false;
                        paddle->color_ = glm::vec3(1.0f);
                    }
                }else if(power_up.type_ == "pass-through"){
                    if(!isOtherPowerUpActive(power_ups_, "pass-through")){
                        ball->pass_through_ = false;
                        ball->color_ = glm::vec3(1.0f);
                    }
                }else if(power_up.type_ == "confuse"){
                    if(!isOtherPowerUpActive(power_ups_, "confuse"))
                        post_processor->confuse_ = false;
                }else if(power_up.type_ == "chaos"){
                    if(!isOtherPowerUpActive(power_ups_, "chaos"))
                        post_processor->chaos_ = false;
                }
            }
        }
    }
    power_ups_.erase(
        std::remove_if(power_ups_.begin(), power_ups_.end(), [](const PowerUp& power_up) {
            return power_up.destroyed_ && !power_up.activated_;
        }), 
        power_ups_.end()
    );
}

