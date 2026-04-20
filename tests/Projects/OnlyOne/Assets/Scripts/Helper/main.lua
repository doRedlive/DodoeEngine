local OnlyOne = {
    moduleName = "OnlyOneMain"
}

local ResourceManager = dodoe.ResourceManager
local Time = dodoe.Time

local SpriteRendererComponent = dodoe.SpriteRendererComponent
local Animation2dComponent = dodoe.Animation2dComponent
local Rigidbody2dComponent = dodoe.Rigidbody2dComponent
local BoxCollider2dComponent = dodoe.BoxCollider2dComponent
local TransformComponent = dodoe.TransformComponent
local TagComponent = dodoe.TagComponent

local RigidbodyBodyType = dodoe.RigidbodyBodyType

local Input = dodoe.Input
local KeyCode = dodoe.KeyCode


local world = dodoe.getWorld()
local scene = world:activeScene()

-- Test --
local test = scene:createEntity("Test")
test:setComponent(TransformComponent, {
    position = { x = 200.0, y = 0.0, z = 0.0 },
    rotation = { x = 0.0, y = 0.0, z = 0.0 },
    scale = { x = 0.5, y = 0.5, z = 1.0 }
})
test:setComponent(Rigidbody2dComponent, {
    body_type = RigidbodyBodyType.dynamic,
    gravity_scale = 0.0
})
test:setComponent(BoxCollider2dComponent, {
    offset = { x = 10.0, y = 10.0},
    size = {x = 10.0, y = 10.0}
})
local test_sr = test:addComponent(SpriteRendererComponent)
test_sr.texture_id = ResourceManager.loadTexture("engine/res/pictures/grm.jpg").textureId

-- Player --
local player = scene:createEntity("LuaPlayer")
player:setComponent(TagComponent, {
    tag = "Player"
})
player:setComponent(TransformComponent, {
    position = { x = 0.0, y = 0.0, z = 0.0 },
    rotation = { x = 0.0, y = 0.0, z = 0.0 },
    scale = { x = 0.5, y = 0.5, z = 1.0 }
})
player:setComponent(Rigidbody2dComponent, {
    body_type = RigidbodyBodyType.dynamic,
    gravity_scale = 0.0
})
local box2d = player:addComponent(BoxCollider2dComponent)
box2d.size.x = 100.0
box2d.size.y = 100.0
local player_sr = player:addComponent(SpriteRendererComponent)
local player_anim = player:addComponent(Animation2dComponent)
local player_idle_textures = {
    ResourceManager.getTexture("player_idle_0", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_0.png").textureId,
    ResourceManager.getTexture("player_idle_1", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_1.png").textureId,
    ResourceManager.getTexture("player_idle_2", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_2.png").textureId,
    ResourceManager.getTexture("player_idle_3", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_3.png").textureId,
    ResourceManager.getTexture("player_idle_4", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_4.png").textureId,
    ResourceManager.getTexture("player_idle_5", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Char 4/with hands/idle_5.png").textureId
}
local player_idle_clip = ResourceManager.createAnimClip2d("player_idle", player_idle_textures, true, 100.0)

player_anim:addAnimClip(player_idle_clip) 
player_sr.texture_id = player_idle_textures[1]

-- Enemy --
---- Enemy Resource ----
local enemy_fly_textures = {
    ResourceManager.loadTexture("enemy_fly_0", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_0.png").textureId,
    ResourceManager.loadTexture("enemy_fly_1", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_1.png").textureId,
    ResourceManager.loadTexture("enemy_fly_2", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_2.png").textureId,
    ResourceManager.loadTexture("enemy_fly_3", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_3.png").textureId,
    ResourceManager.loadTexture("enemy_fly_4", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_4.png").textureId,
    ResourceManager.loadTexture("enemy_fly_5", "engine/src/sandbox/proj/OnlyOne/Pictures/Free 2D Animated Vector Game Character Sprites/Full body animated characters/Enemies/Enemy 3/fly_5.png").textureId
}
local enemy_fly_clip = ResourceManager.createAnimClip2d("enemy_fly_loop", enemy_fly_textures, true, 100.0)

math.randomseed(math.floor(Time.getCurrentTime() * 1000.0))

---- Enemy Function ----
local function createEnemies(count) 
    local enemies = { }
    for i = 1, count do
        local pos_x = -320.0 + math.random() * 640.0
        local pos_y = -180.0 + math.random() * 180.0
        local enemy = scene:createEntity("Enemy")
        enemy:setComponent(TagComponent, {
            tag = "Enemy"
        })
        enemy:setComponent(TransformComponent, {
            position = { x = pos_x, y = pos_y, z = 0.0 },
            scale = { x = 0.5, y = 0.5, z = 0.5}
        })

        local enemy_sr = enemy:addComponent(SpriteRendererComponent)
        local enemy_anim = enemy:addComponent(Animation2dComponent)
        enemy_anim:addAnimClip(enemy_fly_clip)

        enemy_sr.texture_id = enemy_fly_textures[1]

        enemies[i] = enemy
        dodoe.logInfo("Created enemy " .. tostring(i))
    end
    return enemies
end

local function calcDistance(transform1_, transform2_)
    local dx = transform1_.position.x - transform2_.position.x
    local dy = transform1_.position.y - transform2_.position.y
    return math.sqrt(dx * dx + dy * dy)
end

local function calcDirection(transform1_, transform2_)
    local dx = transform1_.position.x - transform2_.position.x
    local dy = transform1_.position.y - transform2_.position.y
    local len = math.sqrt(dx * dx + dy * dy)
    if len <= 0.0001 then
        return { x = 0.0, y = 0.0 }
    end
    return { x = dx / len, y = dy / len }
end

-- TODO: FIXME: Remove this --
local function moveTarget(transform_, direction_, speed_, dt_)
    transform_.position.x = transform_.position.x + direction_.x * speed_ * dt_
    transform_.position.y = transform_.position.y + direction_.y * speed_ * dt_
end

-- TODO: FIXME: Should scene.get_entieies(tag), and use function in the update system 
local function chasePlayer(enemy, player_tr, dt_) 
    local enemy_tr = enemy:getComponent(TransformComponent)
    if enemy_tr == nil or player_tr == nil then
        return
    end
    if calcDistance(player_tr, enemy_tr) < 300.0 then
        local dir = calcDirection(player_tr, enemy_tr)
        moveTarget(enemy_tr, dir, 50.0, dt_)
    end
end


local enemies = createEnemies(5)

local player_rb2d = player:getComponent(Rigidbody2dComponent)
local test_rb2d = test:getComponent(Rigidbody2dComponent)
local speed = 100.0

world:registerSystem({
    function(reg, dt)

        local dir_x = 0.0
        local dir_y = 0.0

        if Input.isKeyPressed(KeyCode.A) then
            dir_x = -1.0
        elseif Input.isKeyPressed(KeyCode.D) then
            dir_x = 1.0
        end

        if Input.isKeyPressed(KeyCode.W) then
            dir_y = 1.0
        elseif Input.isKeyPressed(KeyCode.S) then
            dir_y = -1.0
        end
        
        player_rb2d:setLinearVelocity(dodoe.Vector2f.new(dir_x * speed, dir_y * speed))
        test_rb2d:setLinearVelocity(dodoe.Vector2f.new(dir_x * speed, dir_y * speed))

        for k, v in pairs(enemies) do
            chasePlayer(v, transform, dt)
        end
    end
})

return OnlyOne
