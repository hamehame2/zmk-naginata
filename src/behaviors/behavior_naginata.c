/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_naginata

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>

#include <zmk_naginata/nglist.h>
#include <zmk_naginata/nglistarray.h>
#include <zmk_naginata/naginata_func.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
extern int64_t timestamp;

#define NONE 0

// 薙刀式

// 31キーを32bitの各ビットに割り当てる
#define B_Q (1UL << 0)
#define B_W (1UL << 1)
#define B_E (1UL << 2)
#define B_R (1UL << 3)
#define B_T (1UL << 4)

#define B_Y (1UL << 5)
#define B_U (1UL << 6)
#define B_I (1UL << 7)
#define B_O (1UL << 8)
#define B_P (1UL << 9)

#define B_A (1UL << 10)
#define B_S (1UL << 11)
#define B_D (1UL << 12)
#define B_F (1UL << 13)
#define B_G (1UL << 14)

#define B_H (1UL << 15)
#define B_J (1UL << 16)
#define B_K (1UL << 17)
#define B_L (1UL << 18)
#define B_SEMI (1UL << 19)


#define B_Z (1UL << 20)
#define B_X (1UL << 21)
#define B_C (1UL << 22)
#define B_V (1UL << 23)
#define B_B (1UL << 24)

#define B_N (1UL << 25)
#define B_M (1UL << 26)
#define B_COMMA (1UL << 27)
#define B_DOT (1UL << 28)
#define B_SLASH (1UL << 29)

#define B_SPACE (1UL << 30)
#define B_SQT (1UL << 31)

static NGListArray nginput;
static uint32_t pressed_keys = 0UL; // 押しているキーのビットをたてる
static int8_t n_pressed_keys = 0;   // 押しているキーの数

#define NG_WINDOWS 0
#define NG_MACOS 1
#define NG_LINUX 2
#define NG_IOS 3

// EEPROMに保存する設定
typedef union {
    uint8_t os : 2;  // 2 bits can store values 0-3 (NG_WINDOWS, NG_MACOS, NG_LINUX, NG_IOS)
    bool tategaki : true; // true: 縦書き, false: 横書き
} user_config_t;

extern user_config_t naginata_config;

static const uint32_t ng_key[] = {
    [A - A] = B_A,     [B - A] = B_B,         [C - A] = B_C,         [D - A] = B_D,
    [E - A] = B_E,     [F - A] = B_F,         [G - A] = B_G,         [H - A] = B_H,
    [I - A] = B_I,     [J - A] = B_J,         [K - A] = B_K,         [L - A] = B_L,
    [M - A] = B_M,     [N - A] = B_N,         [O - A] = B_O,         [P - A] = B_P,
    [Q - A] = B_Q,     [R - A] = B_R,         [S - A] = B_S,         [T - A] = B_T,
    [U - A] = B_U,     [V - A] = B_V,         [W - A] = B_W,         [X - A] = B_X,
    [Y - A] = B_Y,     [Z - A] = B_Z,         [SEMI - A] = B_SEMI,   [COMMA - A] = B_COMMA, //[SQT - A] = B_SQT,
    //[COMMA - A] = B_COMMA, [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE,
    //[ENTER - A] = B_SPACE,
    [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE, [ENTER - A] = B_SPACE,
    [SQT - A] = B_SQT,
};

// カナ変換テーブル
typedef struct {
    uint32_t shift;
    uint32_t douji;
    uint32_t kana[6];
    void (*func)(void);
} naginata_kanamap;

static naginata_kanamap ngdickana[] = {
    // 清音/単打
    {.shift = NONE    , .douji = B_J            , .kana = {U, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // う
    {.shift = NONE    , .douji = B_K            , .kana = {I, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // い
    {.shift = NONE    , .douji = B_L            , .kana = {S, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // し
    //{.shift = B_SPACE , .douji = B_O            , .kana = {E, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_N            , .kana = {O, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_F            , .kana = {N, N, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ん
    {.shift = NONE    , .douji = B_W            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // に
    {.shift = NONE    , .douji = B_H            , .kana = {K, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // く
    {.shift = NONE    , .douji = B_S            , .kana = {T, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // と
    {.shift = NONE    , .douji = B_V            , .kana = {R, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // る
    //{.shift = B_SPACE , .douji = B_U            , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_R            , .kana = {COMMA, SPACE, NONE, NONE, NONE, NONE}, .func = nofunc }, // 、変換
    {.shift = NONE    , .douji = B_O            , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    //{.shift = B_SPACE , .douji = B_A            , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_B            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // つ
    {.shift = NONE    , .douji = B_N            , .kana = {T, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // て
    //{.shift = B_SPACE , .douji = B_G            , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_L            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_E            , .kana = {H, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // は
    {.shift = NONE    , .douji = B_D            , .kana = {K, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // か
    {.shift = NONE    , .douji = B_M            , .kana = {T, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // た
    //{.shift = B_SPACE , .douji = B_D            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_W            , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_COMMA        , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_J            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_C            , .kana = {K, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // き
    {.shift = NONE    , .douji = B_X            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ま
    //{.shift = B_SPACE , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_SEMI         , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_P            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = NONE    , .douji = B_Z            , .kana = {S, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // す
    //{.shift = B_SPACE , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_F            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_S            , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_B            , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_R            , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_K            , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_H            , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_P            , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_I            , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_DOT          , .kana = {DOT, ENTER, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 。
    //{.shift = B_SPACE , .douji = B_E            , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_I            , .kana = {K, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // こ
    {.shift = NONE    , .douji = B_SLASH        , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    //{.shift = B_SPACE , .douji = B_SLASH        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_A            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // の
    //{.shift = B_SPACE , .douji = B_DOT          , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    //{.shift = B_SPACE , .douji = B_C            , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // 
    {.shift = NONE    , .douji = B_COMMA        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    {.shift = NONE    , .douji = B_SEMI         , .kana = {N, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // な
    {.shift = NONE    , .douji = B_Q         , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー
    {.shift = NONE    , .douji = B_T         , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ち
    {.shift = NONE    , .douji = B_G         , .kana = {X, T, U, NONE, NONE, NONE   }, .func = nofunc }, // っ
    {.shift = NONE    , .douji = B_Y         , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    {.shift = NONE    , .douji = B_U         , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    // げ追加げはシフトに移行。NG SQTでもいけるようになりましたのでお好みで。
    {.shift = NONE    , .douji = B_SQT         , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ

    // 濁音
    //{.shift = NONE    , .douji = B_J|B_F        , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    //{.shift = NONE    , .douji = B_J|B_W        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    //{.shift = 0UL     , .douji = B_F|B_H        , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    //{.shift = NONE    , .douji = B_J|B_S        , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    //{.shift = NONE    , .douji = B_J|B_V        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    //{.shift = 0UL     , .douji = B_F|B_U        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    //{.shift = NONE    , .douji = B_J|B_R        , .kana = {Z, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    //{.shift = 0UL     , .douji = B_F|B_O        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    //{.shift = NONE    , .douji = B_J|B_A        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    //{.shift = NONE    , .douji = B_J|B_B        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    //{.shift = 0UL     , .douji = B_F|B_N        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    //{.shift = NONE    , .douji = B_J|B_G        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    //{.shift = 0UL     , .douji = B_F|B_L        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    //{.shift = NONE    , .douji = B_J|B_E        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    //{.shift = NONE    , .douji = B_J|B_D        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    //{.shift = NONE    , .douji = B_J|B_C        , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    //{.shift = NONE    , .douji = B_J|B_X        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    //{.shift = 0UL     , .douji = B_F|B_SEMI     , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    //{.shift = 0UL     , .douji = B_F|B_P        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    //{.shift = NONE    , .douji = B_J|B_Z        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    //{.shift = 0UL     , .douji = B_F|B_L|B_SEMI , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ
    
    //中指シフト
    {.shift = NONE    , .douji = B_K|B_Q        , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    {.shift = NONE    , .douji = B_K|B_W        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    {.shift = NONE     , .douji = B_K|B_E        , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふ
    {.shift = NONE    , .douji = B_K|B_R        , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    {.shift = NONE    , .douji = B_K|B_T        , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    {.shift = NONE     , .douji = B_D|B_Y        , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    {.shift = NONE    , .douji = B_D|B_U        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE     , .douji = B_D|B_I        , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // よ
    {.shift = NONE    , .douji = B_D|B_O        , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // み
    {.shift = NONE    , .douji = B_D|B_P        , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    {.shift = NONE    , .douji = B_D|B_SQT     , .kana = {W, H, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    {.shift = NONE    , .douji = B_K|B_A        , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = NONE     , .douji = B_K|B_S        , .kana = {J, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    {.shift = NONE    , .douji = B_K|B_D        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    {.shift = NONE    , .douji = B_K|B_F        , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // も
    {.shift = NONE    , .douji = B_K|B_G        , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゆ
    {.shift = NONE    , .douji = B_D|B_H        , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // へ
    {.shift = NONE     , .douji = B_D|B_J     , .kana = {A, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // あ
    //{.shift = NONE     , .douji = B_D|B_K     , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    //{.shift = NONE     , .douji = B_D|B_L     , .kana = {O, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // お
    {.shift = NONE     , .douji = B_D|B_SEMI        , .kana = {E, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // え
    {.shift = NONE     , .douji = B_K|B_Z        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    {.shift = NONE    , .douji = B_K|B_X        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    {.shift = NONE    , .douji = B_K|B_C        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    {.shift = NONE    , .douji = B_K|B_V        , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // む
    {.shift = NONE    , .douji = B_K|B_B        , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    {.shift = NONE    , .douji = B_D|B_N        , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // せ
    {.shift = NONE    , .douji = B_D|B_M        , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ね
    {.shift = NONE    , .douji = B_D|B_COMMA        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    {.shift = NONE    , .douji = B_D|B_DOT        , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    {.shift = NONE    , .douji = B_D|B_SLASH        , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ


    // 半濁音
    //{.shift = NONE    , .douji = B_M|B_C        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    //{.shift = NONE    , .douji = B_M|B_X        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    //{.shift = NONE    , .douji = B_V|B_SEMI     , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    //{.shift = NONE    , .douji = B_V|B_P        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    //{.shift = NONE    , .douji = B_M|B_Z        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ

    // 薬指シフト
    {.shift = NONE    , .douji = B_L|B_Q        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    {.shift = NONE    , .douji = B_L|B_W        , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // め
    {.shift = NONE    , .douji = B_L|B_E        , .kana = {K, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // け
    {.shift = NONE    , .douji = B_L|B_R        , .kana = {T, H, I, NONE, NONE, NONE   }, .func = nofunc }, // てぃ
    {.shift = NONE    , .douji = B_L|B_T        , .kana = {D, H, I, NONE, NONE, NONE   }, .func = nofunc }, // でぃ
    {.shift = NONE    , .douji = B_S|B_Y        , .kana = {S, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // しぇ
    {.shift = NONE    , .douji = B_S|B_U        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    {.shift = NONE    , .douji = B_S|B_I        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    {.shift = NONE    , .douji = B_S|B_O        , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // や
    {.shift = NONE    , .douji = B_S|B_P        , .kana = {J, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じぇ
    //{.shift = NONE    , .douji = B_S|B_X1        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE    , .douji = B_L|B_A        , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // を
    {.shift = NONE    , .douji = B_L|B_S        , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_L|B_D        , .kana = {O, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // お
    {.shift = NONE    , .douji = B_L|B_F        , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // り
    {.shift = NONE    , .douji = B_L|B_G        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    {.shift = NONE    , .douji = B_S|B_H        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    {.shift = NONE    , .douji = B_S|B_J        , .kana = {R, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ら
    //{.shift = NONE    , .douji = B_S|B_K        , .kana = {J, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    //{.shift = NONE    , .douji = B_S|B_L        , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_S|B_SEMI        , .kana = {S, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // そ
    {.shift = NONE    , .douji = B_L|B_Z        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    {.shift = NONE    , .douji = B_L|B_X        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    {.shift = NONE    , .douji = B_L|B_C        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    {.shift = NONE    , .douji = B_L|B_V        , .kana = {R, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ろ
    {.shift = NONE    , .douji = B_L|B_B        , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぬ
    {.shift = NONE    , .douji = B_S|B_N        , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // わ
    {.shift = NONE    , .douji = B_S|B_M        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    {.shift = NONE    , .douji = B_S|B_COMMA        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    {.shift = NONE    , .douji = B_S|B_DOT        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ
    {.shift = NONE    , .douji = B_S|B_SLASH        , .kana = {T, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ちぇ


    // Iシフト
    {.shift = NONE    , .douji = B_I|B_Q        , .kana = {H, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ひゅ
    {.shift = NONE    , .douji = B_I|B_W        , .kana = {S, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // しゅ
    {.shift = NONE    , .douji = B_I|B_E        , .kana = {S, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // しょ
    {.shift = NONE    , .douji = B_I|B_R        , .kana = {K, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // きゅ
    {.shift = NONE    , .douji = B_I|B_T        , .kana = {T, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ちゅ
    {.shift = NONE    , .douji = B_I|B_A        , .kana = {H, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ひょ
    {.shift = NONE    , .douji = B_I|B_F        , .kana = {K, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // きょ
    {.shift = NONE    , .douji = B_I|B_G        , .kana = {T, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ちょ

    {.shift = NONE    , .douji = B_I|B_Z        , .kana = {H, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ひゃ

    {.shift = NONE    , .douji = B_I|B_C        , .kana = {S, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // しゃ
    {.shift = NONE    , .douji = B_I|B_V        , .kana = {K, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // きゃ
    {.shift = NONE    , .douji = B_I|B_B        , .kana = {T, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ちゃ

    {.shift = NONE    , .douji = B_I|B_X        , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー追加
    // Oシフト
    {.shift = NONE    , .douji = B_O|B_Q        , .kana = {R, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // りゅ
    {.shift = NONE    , .douji = B_O|B_W        , .kana = {J, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // じゅ
    {.shift = NONE    , .douji = B_O|B_E        , .kana = {J, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // じょ
    {.shift = NONE    , .douji = B_O|B_R        , .kana = {G, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ぎゅ
    {.shift = NONE    , .douji = B_O|B_T        , .kana = {N, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // にゅ
    {.shift = NONE    , .douji = B_O|B_A        , .kana = {R, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // りょ
    {.shift = NONE    , .douji = B_O|B_F        , .kana = {G, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ぎょ
    {.shift = NONE    , .douji = B_O|B_G        , .kana = {N, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // にょ

    {.shift = NONE    , .douji = B_O|B_Z        , .kana = {R, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // りゃ

    {.shift = NONE    , .douji = B_O|B_C        , .kana = {Z, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // じゃ
    {.shift = NONE    , .douji = B_O|B_V        , .kana = {G, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ぎゃ
    {.shift = NONE    , .douji = B_O|B_B        , .kana = {N, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // にゃ

    {.shift = NONE    , .douji = B_O|B_X        , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー追加
    // IO3キー同時押しシフト
    {.shift = NONE    , .douji = B_I|B_O|B_Q        , .kana = {P, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ぴゅ
    {.shift = NONE    , .douji = B_I|B_O|B_W        , .kana = {M, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // みゅ
    {.shift = NONE    , .douji = B_I|B_O|B_E        , .kana = {M, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // みょ
    {.shift = NONE    , .douji = B_I|B_O|B_R        , .kana = {B, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // びゅ
    {.shift = NONE    , .douji = B_I|B_O|B_T        , .kana = {D, H, U, NONE, NONE, NONE   }, .func = nofunc }, // でゅ
    {.shift = NONE    , .douji = B_I|B_O|B_A        , .kana = {P, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ぴょ
    {.shift = NONE    , .douji = B_I|B_O|B_F        , .kana = {B, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // びょ
    {.shift = NONE    , .douji = B_I|B_O|B_G        , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ

    {.shift = NONE    , .douji = B_I|B_O|B_Z        , .kana = {P, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ぴゃ

    {.shift = NONE    , .douji = B_I|B_O|B_C        , .kana = {M, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // みゃ
    {.shift = NONE    , .douji = B_I|B_O|B_V        , .kana = {B, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // びゃ
    {.shift = NONE    , .douji = B_I|B_O|B_B        , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ


    {.shift = NONE    , .douji = B_I|B_O|B_X        , .kana = {LS(INT1), NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ＿追加
    // SQTシフト B_SQT 
    {.shift = NONE    , .douji = B_SQT|B_Q        , .kana = {X, K, E, NONE, NONE, NONE   }, .func = nofunc }, // ヶ
    {.shift = NONE    , .douji = B_SQT|B_W        , .kana = {X, K, A, NONE, NONE, NONE   }, .func = nofunc }, // ヵ
    {.shift = NONE    , .douji = B_SQT|B_E        , .kana = {G, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ぎぇ
    {.shift = NONE    , .douji = B_SQT|B_R        , .kana = {P, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ぴぇ
    {.shift = NONE    , .douji = B_SQT|B_T        , .kana = {T, H, A, NONE, NONE, NONE   }, .func = nofunc }, // てゃ
    {.shift = NONE    , .douji = B_SQT|B_A        , .kana = {G, W, A, NONE, NONE, NONE   }, .func = nofunc }, // グァ
    {.shift = NONE    , .douji = B_SQT|B_F        , .kana = {BSPC, BSPC, BSPC, NONE, NONE, NONE   }, .func = nofunc }, // BS3
    {.shift = NONE    , .douji = B_SQT|B_G        , .kana = {G, W, O, NONE, NONE, NONE   }, .func = nofunc }, // ぐぉ

    {.shift = NONE    , .douji = B_SQT|B_Z        , .kana = {G, U, X, W, A, NONE   }, .func = nofunc }, // ぐゎ

    {.shift = NONE    , .douji = B_SQT|B_X        , .kana = {G, E, X, E, NONE, NONE   }, .func = nofunc }, // げぇ
    {.shift = NONE    , .douji = B_SQT|B_C        , .kana = {G, W, E, NONE, NONE, NONE   }, .func = nofunc }, // ぐぇ
    {.shift = NONE    , .douji = B_SQT|B_V        , .kana = {T, H, E, NONE, NONE, NONE   }, .func = nofunc }, // てぇ
    {.shift = NONE    , .douji = B_SQT|B_B        , .kana = {D, H, E, NONE, NONE, NONE   }, .func = nofunc }, // でぇ

    {.shift = NONE    , .douji = B_SQT|B_S        , .kana = {K, W, A, NONE, NONE, NONE   }, .func = nofunc }, // クァ

    // 小書き
    //{.shift = NONE    , .douji = B_Q|B_H        , .kana = {X, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ゃ
    //{.shift = NONE    , .douji = B_Q|B_P        , .kana = {X, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ゅ
    //{.shift = NONE    , .douji = B_Q|B_I        , .kana = {X, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ょ
    //{.shift = NONE    , .douji = B_Q|B_J        , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    //{.shift = NONE    , .douji = B_Q|B_K        , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    //{.shift = NONE    , .douji = B_Q|B_L        , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    //{.shift = NONE    , .douji = B_Q|B_O        , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    //{.shift = NONE    , .douji = B_Q|B_N        , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    //{.shift = NONE    , .douji = B_Q|B_DOT      , .kana = {X, W, A, NONE, NONE, NONE      }, .func = nofunc }, // ゎ
    //{.shift = NONE    , .douji = B_G            , .kana = {X, T, U, NONE, NONE, NONE      }, .func = nofunc }, // っ
    //{.shift = NONE    , .douji = B_Q|B_S        , .kana = {X, K, E, NONE, NONE, NONE      }, .func = nofunc }, // ヶ入れなくていいよね
    //{.shift = NONE    , .douji = B_Q|B_F        , .kana = {X, K, A, NONE, NONE, NONE      }, .func = nofunc }, // ヵ入れなくていいよね

    // 清音拗音 濁音拗音 半濁拗音
    //{.shift = NONE    , .douji = B_R|B_H        , .kana = {S, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // しゃ
    //{.shift = NONE    , .douji = B_R|B_P        , .kana = {S, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // しゅ
    //{.shift = NONE    , .douji = B_R|B_I        , .kana = {S, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // しょ
    //{.shift = NONE    , .douji = B_J|B_R|B_H    , .kana = {Z, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // じゃ
    //{.shift = NONE    , .douji = B_J|B_R|B_P    , .kana = {Z, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // じゅ
    //{.shift = NONE    , .douji = B_J|B_R|B_I    , .kana = {Z, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // じょ
    //{.shift = NONE    , .douji = B_W|B_H        , .kana = {K, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // きゃ
    //{.shift = NONE    , .douji = B_W|B_P        , .kana = {K, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // きゅ
    //{.shift = NONE    , .douji = B_W|B_I        , .kana = {K, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // きょ
    //{.shift = NONE    , .douji = B_J|B_W|B_H    , .kana = {G, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぎゃ
    //{.shift = NONE    , .douji = B_J|B_W|B_P    , .kana = {G, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぎゅ
    //{.shift = NONE    , .douji = B_J|B_W|B_I    , .kana = {G, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぎょ
    //{.shift = NONE    , .douji = B_G|B_H        , .kana = {T, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ちゃ
    //{.shift = NONE    , .douji = B_G|B_P        , .kana = {T, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ちゅ
    //{.shift = NONE    , .douji = B_G|B_I        , .kana = {T, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ちょ
    //{.shift = NONE    , .douji = B_J|B_G|B_H    , .kana = {D, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぢゃいるこれ？
    //{.shift = NONE    , .douji = B_J|B_G|B_P    , .kana = {D, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぢゅいるこれ？
    //{.shift = NONE    , .douji = B_J|B_G|B_I    , .kana = {D, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぢょいるこれ？
    //{.shift = NONE    , .douji = B_D|B_H        , .kana = {N, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // にゃ
    //{.shift = NONE    , .douji = B_D|B_P        , .kana = {N, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // にゅ
    //{.shift = NONE    , .douji = B_D|B_I        , .kana = {N, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // にょ
    //{.shift = NONE    , .douji = B_X|B_H        , .kana = {H, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ひゃ
    //{.shift = NONE    , .douji = B_X|B_P        , .kana = {H, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ひゅ
    //{.shift = NONE    , .douji = B_X|B_I        , .kana = {H, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ひょ
    //{.shift = NONE    , .douji = B_J|B_X|B_H    , .kana = {B, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // びゃ
    //{.shift = NONE    , .douji = B_J|B_X|B_P    , .kana = {B, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // びゅ
    //{.shift = NONE    , .douji = B_J|B_X|B_I    , .kana = {B, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // びょ
    //{.shift = NONE    , .douji = B_M|B_X|B_H    , .kana = {P, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぴゃ
    //{.shift = NONE    , .douji = B_M|B_X|B_P    , .kana = {P, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぴゅ
    //{.shift = NONE    , .douji = B_M|B_X|B_I    , .kana = {P, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぴょ
    //{.shift = NONE    , .douji = B_S|B_H        , .kana = {M, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // みゃ
    //{.shift = NONE    , .douji = B_S|B_P        , .kana = {M, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // みゅ
    //{.shift = NONE    , .douji = B_S|B_I        , .kana = {M, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // みょ
    //{.shift = NONE    , .douji = B_E|B_H        , .kana = {R, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // りゃ
    //{.shift = NONE    , .douji = B_E|B_P        , .kana = {R, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // りゅ
    //{.shift = NONE    , .douji = B_E|B_I        , .kana = {R, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // りょ

    // 清音外来音 濁音外来音
    //{.shift = NONE    , .douji = B_M|B_E|B_K    , .kana = {T, H, I, NONE, NONE, NONE      }, .func = nofunc }, // てぃ
    //{.shift = NONE    , .douji = B_M|B_E|B_P    , .kana = {T, E, X, Y, U, NONE            }, .func = nofunc }, // てゅ
    //{.shift = NONE    , .douji = B_J|B_E|B_K    , .kana = {D, H, I, NONE, NONE, NONE      }, .func = nofunc }, // でぃ
    //{.shift = NONE    , .douji = B_J|B_E|B_P    , .kana = {D, H, U, NONE, NONE, NONE      }, .func = nofunc }, // でゅ
    //{.shift = NONE    , .douji = B_M|B_D|B_L    , .kana = {T, O, X, U, NONE, NONE         }, .func = nofunc }, // とぅ
    //{.shift = NONE    , .douji = B_J|B_D|B_L    , .kana = {D, O, X, U, NONE, NONE         }, .func = nofunc }, // どぅ
    //{.shift = NONE    , .douji = B_M|B_R|B_O    , .kana = {S, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // しぇ
    //{.shift = NONE    , .douji = B_M|B_G|B_O    , .kana = {T, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ちぇ
    //{.shift = NONE    , .douji = B_J|B_R|B_O    , .kana = {Z, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // じぇ
    //{.shift = NONE    , .douji = B_J|B_G|B_O    , .kana = {D, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ぢぇnoneed
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_J , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_K , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_O , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_N , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    //{.shift = NONE    , .douji = B_V|B_SEMI|B_P , .kana = {F, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ふゅ一応あるから
    //{.shift = NONE    , .douji = B_V|B_K|B_O    , .kana = {I, X, E, NONE, NONE, NONE      }, .func = nofunc }, // いぇはない
    //{.shift = NONE    , .douji = B_V|B_L|B_K    , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    //{.shift = NONE    , .douji = B_V|B_L|B_O    , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    //{.shift = NONE    , .douji = B_V|B_L|B_N    , .kana = {U, X, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    //{.shift = NONE    , .douji = B_F|B_L|B_J    , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ
    //{.shift = NONE    , .douji = B_F|B_L|B_K    , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ
    //{.shift = NONE    , .douji = B_F|B_L|B_O    , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぇはないnoneed
    //{.shift = NONE    , .douji = B_F|B_L|B_N    , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉはないnoneed
    //{.shift = NONE    , .douji = B_F|B_L|B_P    , .kana = {V, U, X, Y, U, NONE            }, .func = nofunc }, // ゔゅはない
    //{.shift = NONE    , .douji = B_V|B_H|B_J    , .kana = {K, U, X, A, NONE, NONE         }, .func = nofunc }, // くぁはないnoneed
    //{.shift = NONE    , .douji = B_V|B_H|B_K    , .kana = {K, U, X, I, NONE, NONE         }, .func = nofunc }, // くぃはない
    //{.shift = NONE    , .douji = B_V|B_H|B_O    , .kana = {K, U, X, E, NONE, NONE         }, .func = nofunc }, // くぇはない
    //{.shift = NONE    , .douji = B_V|B_H|B_N    , .kana = {K, U, X, O, NONE, NONE         }, .func = nofunc }, // くぉはない
    //{.shift = NONE    , .douji = B_V|B_H|B_DOT  , .kana = {K, U, X, W, A, NONE            }, .func = nofunc }, // くゎはない
    //{.shift = NONE    , .douji = B_F|B_H|B_J    , .kana = {G, U, X, A, NONE, NONE         }, .func = nofunc }, // ぐぁはないいらないんじゃ
    //{.shift = NONE    , .douji = B_F|B_H|B_K    , .kana = {G, U, X, I, NONE, NONE         }, .func = nofunc }, // ぐぃいらない
    //{.shift = NONE    , .douji = B_F|B_H|B_O    , .kana = {G, U, X, E, NONE, NONE         }, .func = nofunc }, // ぐぇはないいらないんじゃ
    //{.shift = NONE    , .douji = B_F|B_H|B_N    , .kana = {G, U, X, O, NONE, NONE         }, .func = nofunc }, // ぐぉいらない
    //{.shift = NONE    , .douji = B_F|B_H|B_DOT  , .kana = {G, U, X, W, A, NONE            }, .func = nofunc }, // ぐゎはないnoneed
    //{.shift = NONE    , .douji = B_V|B_L|B_J    , .kana = {T, S, A, NONE, NONE, NONE      }, .func = nofunc }, // つぁはない

    // げうぉ外来音とネットスラングを親指のNGスペースにて
    {.shift = B_SPACE , .douji = B_Q            , .kana = {X, W, A, NONE, NONE, NONE   }, .func = nofunc }, // ゎ
    {.shift = B_SPACE , .douji = B_W            , .kana = {X, Y, A, NONE, NONE, NONE   }, .func = nofunc }, // ゃ
    {.shift = B_SPACE , .douji = B_E            , .kana = {X, Y, O, NONE, NONE, NONE   }, .func = nofunc }, // ょ
    {.shift = B_SPACE , .douji = B_R            , .kana = {X, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ゅ
    {.shift = B_SPACE , .douji = B_T            , .kana = {T, H, U, NONE, NONE, NONE   }, .func = nofunc }, // てゅ
    {.shift = B_SPACE , .douji = B_Y            , .kana = {S, U, X, I, NONE, NONE   }, .func = nofunc }, // すぃ
    {.shift = B_SPACE , .douji = B_U            , .kana = {D, W, U, NONE, NONE, NONE   }, .func = nofunc }, // どぅ
    {.shift = B_SPACE , .douji = B_I            , .kana = {Q, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぇ
    {.shift = B_SPACE , .douji = B_O            , .kana = {T, S, E, NONE, NONE, NONE   }, .func = nofunc }, // ツェ
    {.shift = B_SPACE , .douji = B_P            , .kana = {H, Y, E, NONE, NONE, NONE   }, .func = nofunc }, // ひぇ
    {.shift = B_SPACE , .douji = B_A            , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    {.shift = B_SPACE , .douji = B_S           , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    {.shift = B_SPACE , .douji = B_D           , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    {.shift = B_SPACE , .douji = B_F           , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    {.shift = B_SPACE , .douji = B_G           , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    {.shift = B_SPACE , .douji = B_H           , .kana = {W, H, O, NONE, NONE, NONE   }, .func = nofunc }, // うぉ
    {.shift = B_SPACE , .douji = B_J           , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ー
    {.shift = B_SPACE , .douji = B_K           , .kana = {Q, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぉ
    {.shift = B_SPACE , .douji = B_L           , .kana = {T, S, O, NONE, NONE, NONE   }, .func = nofunc }, // つぉ
    {.shift = B_SPACE , .douji = B_SEMI           , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    {.shift = B_SPACE , .douji = B_SQT          , .kana = {G, E, MINUS, NONE, NONE, NONE   }, .func = nofunc }, // げー
    {.shift = B_SPACE , .douji = B_Z           , .kana = {K, U, X, W, A, NONE   }, .func = nofunc }, // くゎ
    {.shift = B_SPACE , .douji = B_X           , .kana = {Y, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // いぇ
    {.shift = B_SPACE , .douji = B_C           , .kana = {V, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // ゔゅ
    {.shift = B_SPACE , .douji = B_V           , .kana = {F, Y, U, NONE, NONE, NONE   }, .func = nofunc }, // フュ
    {.shift = B_SPACE , .douji = B_B           , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉ
    {.shift = B_SPACE , .douji = B_N           , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぇ
    {.shift = B_SPACE , .douji = B_M           , .kana = {T, W, U, NONE, NONE, NONE   }, .func = nofunc }, // とぅ
    {.shift = B_SPACE , .douji = B_COMMA           , .kana = {Q, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // くぃ
    {.shift = B_SPACE , .douji = B_DOT           , .kana = {T, S, I, NONE, NONE, NONE   }, .func = nofunc }, // つぃ
    {.shift = B_SPACE , .douji = B_SLASH           , .kana = {T, S, A, NONE, NONE, NONE   }, .func = nofunc }, // つぁ

    // 追加
    {.shift = NONE    , .douji = B_SPACE        , .kana = {SPACE, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc},
    //{.shift = B_SPACE , .douji = B_V            , .kana = {COMMA, ENTER, NONE, NONE, NONE, NONE }, .func = nofunc},
    //{.shift = NONE    , .douji = B_Q            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},
    //{.shift = B_SPACE , .douji = B_M            , .kana = {DOT, ENTER, NONE, NONE, NONE, NONE   }, .func = nofunc},
    //{.shift = NONE    , .douji = B_U            , .kana = {BSPC, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc},

    {.shift = NONE    , .douji = B_V|B_M        , .kana = {ENTER, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc}, // enter
    // {.shift = B_SPACE, .douji = B_V|B_M, .kana = {ENTER, NONE, NONE, NONE, NONE, NONE}, .func = nofunc}, // enter+シフト(連続シフト)

    //{.shift = NONE    , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_T}, //
    //{.shift = NONE    , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_Y}, //
    //{.shift = B_SPACE , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_ST}, //
    //{.shift = B_SPACE , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_SY}, //

{.shift = NONE    , .douji = B_H|B_J        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = naginata_on}, // 　かなオン
    // {.shift = NONE, .douji = B_F | B_G, .kana = {NONE, NONE, NONE, NONE, NONE, NONE}, .func = naginata_off}, // 　かなオフ

    // 編集モード
    {.shift = B_J|B_K    , .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKQ    }, // ^{End}
    {.shift = B_J|B_K    , .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKW    }, // ／{改行}
    {.shift = B_J|B_K    , .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKE    }, // /*ディ*/
    {.shift = B_J|B_K    , .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKR    }, // ^s
    {.shift = B_J|B_K    , .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKT    }, // ・
    {.shift = B_J|B_K    , .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKA    }, // ……{改行}
    //{.shift = B_J|B_K    , .douji = B_A     , .kana = {RBKT, BSLH, ENTER, LEFT, NONE, NONE} , .func = nofunc    }, // 「」
    {.shift = B_J|B_K    , .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKS    }, // 『{改行}
    //{.shift = B_J|B_K    , .douji = B_S     , .kana = {LS(N8), LS(N9), ENTER, LEFT, NONE, NONE} , .func = nofunc    }, // （）
    {.shift = B_J|B_K    , .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKD    }, // ？{改行}
    {.shift = B_J|B_K    , .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKF    }, // 「{改行}
    {.shift = B_J|B_K    , .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKG    }, // ({改行}
    //{.shift = B_J|B_K    , .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKZ    }, // ――{改行}
    {.shift = B_J|B_K    , .douji = B_Z     , .kana = {LS(N5), NONE, NONE, NONE, NONE, NONE} , .func = nofunc    }, // %
    //{.shift = B_J|B_K    , .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKX    }, // 』{改行}
    {.shift = B_J|B_K    , .douji = B_X     , .kana = {LS(EQUAL), NONE, NONE, NONE, NONE, NONE} , .func = nofunc    }, // ～
    {.shift = B_J|B_K    , .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKC    }, // ！{改行}
    {.shift = B_J|B_K    , .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKV    }, // 」{改行}
    {.shift = B_J|B_K    , .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKB    }, // ){改行}
    {.shift = B_D|B_F    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFY    }, // {Home}
    {.shift = B_D|B_F    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFU    }, // +{End}{BS}
    {.shift = B_D|B_F    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFI    }, // {vk1Csc079}
    //{.shift = B_D|B_F    , .douji = B_I     , .kana = {LWIN, SLASH, NONE, NONE, NONE, NONE} , .func = nofunc    }, // {vk1Csc079}
    {.shift = B_D|B_F    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFO    }, // {Del}
    {.shift = B_D|B_F    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFP    }, // +{Esc 2}
    {.shift = B_D|B_F    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFH    }, // {Enter}{End}
    {.shift = B_D|B_F    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFJ    }, // {↑}
    {.shift = B_D|B_F    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFK    }, // +{↑}
    {.shift = B_D|B_F    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFL    }, // +{↑ 7}
    //{.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSCLN }, // ^i
    {.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {F7, NONE, NONE, NONE, NONE, NONE} , .func = nofunc }, // ^i
    {.shift = B_D|B_F    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFN    }, // {End}
    {.shift = B_D|B_F    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFM    }, // {↓}
    {.shift = B_D|B_F    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFCOMM }, // +{↓}
    {.shift = B_D|B_F    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFDOT  }, // +{↓ 7}
    //{.shift = B_D|B_F    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSLSH }, // ^u
    {.shift = B_D|B_F    , .douji = B_SLASH  , .kana = {F6, NONE, NONE, NONE, NONE, NONE} , .func = nofunc }, // ^u
    {.shift = B_M|B_COMMA, .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCQ    }, // ｜{改行}
    {.shift = B_M|B_COMMA, .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCW    }, // 　　　×　　　×　　　×{改行 2}
    {.shift = B_M|B_COMMA, .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCE    }, // {Home}{→}{End}{Del 2}{←}
    {.shift = B_M|B_COMMA, .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCR    }, // {Home}{改行}{Space 1}{←}
    {.shift = B_M|B_COMMA, .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCT    }, // 〇{改行}
    {.shift = B_M|B_COMMA, .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCA    }, // 《{改行}
    {.shift = B_M|B_COMMA, .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCS    }, // 【{改行}
    {.shift = B_M|B_COMMA, .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCD    }, // {Home}{→}{End}{Del 4}{←}
    {.shift = B_M|B_COMMA, .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCF    }, // {Home}{改行}{Space 3}{←}
    {.shift = B_M|B_COMMA, .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCG    }, // {Space 3}
    {.shift = B_M|B_COMMA, .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCZ    }, // 》{改行}
    {.shift = B_M|B_COMMA, .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCX    }, // 】{改行}
    {.shift = B_M|B_COMMA, .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCC    }, // 」{改行}{改行}
    {.shift = B_M|B_COMMA, .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCV    }, // 」{改行}{改行}「{改行}
    {.shift = B_M|B_COMMA, .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCB    }, // 」{改行}{改行}{Space}
    {.shift = B_C|B_V    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVY    }, // +{Home}
    {.shift = B_C|B_V    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVU    }, // ^x
    {.shift = B_C|B_V    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVI    }, // {vk1Csc079}
    {.shift = B_C|B_V    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVO    }, // ^v
    {.shift = B_C|B_V    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVP    }, // ^z
    {.shift = B_C|B_V    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVH    }, // ^c
    {.shift = B_C|B_V    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVJ    }, // {←}
    {.shift = B_C|B_V    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVK    }, // {→}
    {.shift = B_C|B_V    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVL    }, // {改行}{Space}+{Home}^x{BS}
    {.shift = B_C|B_V    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSCLN }, // ^y
    {.shift = B_C|B_V    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVN    }, // +{End}
    {.shift = B_C|B_V    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVM    }, // +{←}
    {.shift = B_C|B_V    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVCOMM }, // +{→}
    {.shift = B_C|B_V    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVDOT  }, // +{← 7}
    {.shift = B_C|B_V    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSLSH }, // +{→ 7}

};

// Helper function for counting matches/candidates
static int count_kana_entries(NGList *keys, bool exact_match) {
  if (keys->size == 0) return 0;

  int count = 0;
  uint32_t keyset0 = 0UL, keyset1 = 0UL, keyset2 = 0UL;
  
  // keysetを配列にしたらバイナリサイズが増えた
  switch (keys->size) {
    case 1:
      keyset0 = ng_key[keys->elements[0] - A];
      break;
    case 2:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      break;
    default:
      keyset0 = ng_key[keys->elements[0] - A];
      keyset1 = ng_key[keys->elements[1] - A];
      keyset2 = ng_key[keys->elements[2] - A];
      break;
  }

  for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[i]; i++) {
    bool matches = false;

    switch (keys->size) {
      case 1:
        if (exact_match) {
          matches = (ngdickana[i].shift == keyset0) || 
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == keyset0);
        } else {
          matches = ((ngdickana[i].shift & keyset0) == keyset0) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & keyset0) == keyset0);
        }
        break;
      case 2:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == keyset1) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1)) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & keyset1) == keyset1) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1)) == (keyset0 | keyset1));
          // しぇ、ちぇ、など2キーで確定してはいけない
          if (matches && (ngdickana[i].shift | ngdickana[i].douji) != (keyset0 | keyset1)) {
            count = 2;
          }
        }
        break;
      default:
        if (exact_match) {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && ngdickana[i].douji == keyset2) ||
                   (ngdickana[i].shift == keyset0 && ngdickana[i].douji == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == (keyset0 | keyset1 | keyset2));
        } else {
          matches = (ngdickana[i].shift == (keyset0 | keyset1) && (ngdickana[i].douji & keyset2) == keyset2) ||
                   (ngdickana[i].shift == keyset0 && (ngdickana[i].douji & (keyset1 | keyset2)) == (keyset1 | keyset2)) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & (keyset0 | keyset1 | keyset2)) == (keyset0 | keyset1 | keyset2));
        }
        break;
    }

    if (matches) {
      count++;
      if (count > 1) break;
    }
  }

  return count;
}

int number_of_matches(NGList *keys) {  
  int result = count_kana_entries(keys, true);
  return result;
}

int number_of_candidates(NGList *keys) {
  int result = count_kana_entries(keys, false);
  return result;
}

// キー入力を文字に変換して出力する
void ng_type(NGList *keys) {
    LOG_DBG(">NAGINATA NG_TYPE");

    if (keys->size == 0)
        return;

    if (keys->size == 1 && keys->elements[0] == ENTER) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", ENTER);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        return;
    }

    uint32_t keyset = 0UL;
    for (int i = 0; i < keys->size; i++) {
        keyset |= ng_key[keys->elements[i] - A];
    }

    for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
        if ((ngdickana[i].shift | ngdickana[i].douji) == keyset) {
            if (ngdickana[i].kana[0] != NONE) {
                for (int k = 0; k < 6; k++) {
                    if (ngdickana[i].kana[k] == NONE)
                        break;
                    LOG_DBG(" NAGINATA type keycode 0x%02X", ngdickana[i].kana[k]);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], true,
                                                                 timestamp);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], false,
                                                                 timestamp);
                }
            } else {
                ngdickana[i].func();
            }
            LOG_DBG("<NAGINATA NG_TYPE");
            return;
        }
    }

    // JIみたいにJIを含む同時押しはたくさんあるが、JIのみの同時押しがないとき
    // 最後の１キーを別に分けて変換する
    NGList a, b;
    initializeList(&a);
    initializeList(&b);
    for (int i = 0; i < keys->size - 1; i++) {
        addToList(&a, keys->elements[i]);
    }
    addToList(&b, keys->elements[keys->size - 1]);
    ng_type(&a);
    ng_type(&b);

    LOG_DBG("<NAGINATA NG_TYPE");
}

// 薙刀式の入力処理
bool naginata_press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA PRESS");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case SQT:        
        n_pressed_keys++;
        pressed_keys |= ng_key[keycode - A]; // キーの重ね合わせ

        if (keycode == SPACE || keycode == ENTER) {
            NGList a;
            initializeList(&a);
            addToList(&a, keycode);
            addToListArray(&nginput, &a);
        } else {
            NGList a;
            NGList b;
            if (nginput.size > 0) {
                copyList(&(nginput.elements[nginput.size - 1]), &a);
                copyList(&a, &b);
                addToList(&b, keycode);
            }

            // 前のキーとの同時押しの可能性があるなら前に足す
            // 同じキー連打を除外
            if (nginput.size > 0 && a.elements[a.size - 1] != keycode &&
                number_of_candidates(&b) > 0) {
                removeFromListArrayAt(&nginput, nginput.size - 1);
                addToListArray(&nginput, &b);
                // 前のキーと同時押しはない
            } else {
                // 連続シフトではない
                NGList e;
                initializeList(&e);
                addToList(&e, keycode);
                addToListArray(&nginput, &e);
            }
        }

        // 連続シフト
        static uint32_t rs[10][2] = {{D, F},     {C, V}, {J, K}, {M, COMMA}, {SPACE, 0},
                                     {ENTER, 0}, {F, 0}, {V, 0}, {J, 0},     {M, 0}};

        uint32_t keyset = 0UL;
        for (int i = 0; i < nginput.elements[0].size; i++) {
            keyset |= ng_key[nginput.elements[0].elements[i] - A];
        }
        for (int i = 0; i < 10; i++) {
            NGList rskc;
            initializeList(&rskc);
            addToList(&rskc, rs[i][0]);
            if (rs[i][1] > 0) {
                addToList(&rskc, rs[i][1]);
            }

            int c = includeList(&rskc, keycode);
            uint32_t brs = 0UL;
            for (int j = 0; j < rskc.size; j++) {
                brs |= ng_key[rskc.elements[j] - A];
            }

            NGList l = nginput.elements[nginput.size - 1];
            for (int j = 0; j < l.size; j++) {
                addToList(&rskc, l.elements[j]);
            }

            if (c < 0 && ((brs & pressed_keys) == brs) && (keyset & brs) != brs && number_of_matches(&rskc) > 0) {
                nginput.elements[nginput.size - 1] = rskc;
                break;
            }
        }

        if (nginput.size > 1 || number_of_candidates(&(nginput.elements[0])) == 1) {
            ng_type(&(nginput.elements[0]));
            removeFromListArrayAt(&nginput, 0);
        }
        break;
    }

    LOG_DBG("<NAGINATA PRESS");

    return true;
}

bool naginata_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA RELEASE");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case SQT:
        if (n_pressed_keys > 0)
            n_pressed_keys--;
        if (n_pressed_keys == 0)
            pressed_keys = 0UL;

        pressed_keys &= ~ng_key[keycode - A]; // キーの重ね合わせ

        if (pressed_keys == 0UL) {
            while (nginput.size > 0) {
                ng_type(&(nginput.elements[0]));
                removeFromListArrayAt(&nginput, 0);
            }
        } else {
            if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
                ng_type(&(nginput.elements[0]));
                removeFromListArrayAt(&nginput, 0);
            }
        }
        break;
    }

    LOG_DBG("<NAGINATA RELEASE");

    return true;
}

// 薙刀式

static int behavior_naginata_init(const struct device *dev) {
    LOG_DBG("NAGINATA INIT");

    initializeListArray(&nginput);
    pressed_keys = 0UL;
    n_pressed_keys = 0;
    naginata_config.os =  NG_MACOS;

    return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    // F15が押されたらnaginata_config.os=NG_WINDOWS
    switch (binding->param1) {
        case F15:
            naginata_config.os = NG_WINDOWS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F16:
            naginata_config.os = NG_MACOS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F17:
            naginata_config.os = NG_LINUX;
            return ZMK_BEHAVIOR_OPAQUE;
        case F18:
            naginata_config.tategaki = true;
            return ZMK_BEHAVIOR_OPAQUE;
        case F19:
            naginata_config.tategaki = false;
            return ZMK_BEHAVIOR_OPAQUE;
    }

    timestamp = event.timestamp;
    naginata_press(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    timestamp = event.timestamp;
    naginata_release(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_naginata_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_naginata_init, NULL, NULL, NULL, POST_KERNEL,              \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_naginata_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)
