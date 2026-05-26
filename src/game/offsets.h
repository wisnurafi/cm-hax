// game/offsets.h
// Game-specific field offsets. Tied to a specific Project.dll build.
// Re-dump and diff with Il2CppDumper after every game patch.
#pragma once

// PlayerRoot
#define OFF_PLAYERROOT_HEALTH               0xB8
#define OFF_PLAYERROOT_ISOWNER              0x108
#define OFF_PLAYERROOT_ISREAL               0x132
#define OFF_PLAYERROOT_HEADHITBOX           0x58
#define OFF_PLAYERROOT_THIRDPERSONVIEW      0x88
#define OFF_PLAYERROOT_HITBOXBODY           0x90
#define OFF_PLAYERROOT_CHARACTERCONTROLLER  0xA0
#define OFF_PLAYERROOT_MOVEMENT             0xB0
#define OFF_PLAYERROOT_CACHEDPLAYERDATA     0x150

// PlayerHealth
#define OFF_PLAYERHEALTH_HP                 0xD0
#define OFF_PLAYERHEALTH_ARMOR              0xCC

// PlayerMobView (third-person bone references)
#define OFF_PLAYERMOBVIEW_HEADTRANSFORM     0x40
#define OFF_PLAYERMOBVIEW_NECKTRANSFORM     0x48
#define OFF_PLAYERMOBVIEW_CHESTTRANSFORM    0x58
#define OFF_PLAYERMOBVIEW_SPINETRANSFORM    0x60
#define OFF_PLAYERMOBVIEW_LKNEETRANSFORM    0x68
#define OFF_PLAYERMOBVIEW_RKNEETRANSFORM    0x70

// Bolt hitboxes
#define OFF_BOLTHITBOX_CENTER               0x28
#define OFF_BOLTHITBOX_BOXSIZE              0x34
#define OFF_BOLTHITBOXBODY_HITBOXES         0x30

// PlayerMovement (currently unused at runtime but kept for reference)
#define OFF_PLAYERMOVEMENT_VELOCITY         0x80
#define OFF_PLAYERMOVEMENT_ISINAIR          0x94
#define OFF_PLAYERMOVEMENT_ISFALLING        0x96
#define OFF_PLAYERMOVEMENT_ISMOVEUPINPUT    0xA5
#define OFF_PLAYERMOVEMENT_GRAVITY          0xBC
#define OFF_PLAYERMOVEMENT_ISGROUNDED       0xC0
