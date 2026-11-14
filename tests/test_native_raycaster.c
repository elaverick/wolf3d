#ifdef NDEBUG
#undef NDEBUG
#endif

#include "WL_DEF.H"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void AsmRefresh(void);

int midangle;
int focaltx;
int focalty;

unsigned xpartialup;
unsigned ypartialup;
unsigned xpartialdown;
unsigned ypartialdown;

unsigned tilehit;
unsigned pixx;
int xtile;
int ytile;
int xtilestep;
int ytilestep;
fixed xintercept;
fixed yintercept;
fixed xstep;
fixed ystep;

byte tilemap[MAPSIZE][MAPSIZE];
byte spotvis[MAPSIZE][MAPSIZE];
unsigned doorposition[MAXDOORS];
unsigned pwallpos;
int pixelangle[MAXVIEWWIDTH];
fixed finetangent[FINEANGLES / 4];
fixed viewx;
fixed viewy;
int viewwidth;

static unsigned vertical_hits;
static unsigned horizontal_hits;
static unsigned vertical_door_hits;
static unsigned horizontal_door_hits;
static unsigned vertical_pushwall_hits;
static unsigned horizontal_pushwall_hits;

void HitHorizWall(void) { ++horizontal_hits; }
void HitVertWall(void) { ++vertical_hits; }
void HitHorizDoor(void) { ++horizontal_door_hits; }
void HitVertDoor(void) { ++vertical_door_hits; }
void HitHorizPWall(void) { ++horizontal_pushwall_hits; }
void HitVertPWall(void) { ++vertical_pushwall_hits; }

static void reset_ray_state(void)
{
    memset(tilemap, 0, sizeof(tilemap));
    memset(spotvis, 0, sizeof(spotvis));
    memset(pixelangle, 0, sizeof(pixelangle));

    midangle = 0;
    focaltx = 10;
    focalty = 10;
    xpartialup = 0;
    ypartialup = 0;
    xpartialdown = 0;
    ypartialdown = 0;
    viewx = (fixed)(10 * TILEGLOBAL + TILEGLOBAL / 2);
    viewy = (fixed)(10 * TILEGLOBAL + TILEGLOBAL / 2);
    viewwidth = 1;

    vertical_hits = 0;
    horizontal_hits = 0;
    vertical_door_hits = 0;
    horizontal_door_hits = 0;
    vertical_pushwall_hits = 0;
    horizontal_pushwall_hits = 0;
}

static void set_uniform_ray_tangent(fixed tangent)
{
    size_t i;

    for (i = 0; i < sizeof(finetangent) / sizeof(finetangent[0]); ++i) {
        finetangent[i] = tangent;
    }
}

static void test_four_quadrants_hit_expected_vertical_wall(void)
{
    static const struct {
        int angle;
        int wall_x;
        int wall_y;
    } cases[] = {
        {450, 11, 10},
        {1350, 9, 10},
        {2250, 9, 10},
        {3150, 11, 10}
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset_ray_state();
        midangle = cases[i].angle;
        tilemap[cases[i].wall_x][cases[i].wall_y] = 1;

        AsmRefresh();

        assert(vertical_hits == 1);
        assert(horizontal_hits == 0);
        assert(pixx == 1);
    }
}

static void test_ray_stops_at_map_boundary(void)
{
    reset_ray_state();
    focaltx = MAPSIZE - 1;
    viewx = (fixed)((MAPSIZE - 1) * TILEGLOBAL + TILEGLOBAL / 2);

    AsmRefresh();

    assert(vertical_hits == 0);
    assert(horizontal_hits == 0);
    assert(pixx == 1);
}

static void test_vertical_special_tile_dispatch(void)
{
    reset_ray_state();
    set_uniform_ray_tangent(65536);
    midangle = 450;
    tilemap[11][10] = (byte)(0x80u | 5u);
    doorposition[5] = 0;

    AsmRefresh();

    assert(vertical_door_hits == 1);
    assert(vertical_pushwall_hits == 0);
    assert(horizontal_door_hits == 0);
    assert(horizontal_pushwall_hits == 0);

    reset_ray_state();
    set_uniform_ray_tangent(65536);
    midangle = 450;
    tilemap[11][10] = (byte)(0xc0u | 5u);
    pwallpos = 0;

    AsmRefresh();

    assert(vertical_pushwall_hits == 1);
    assert(vertical_door_hits == 0);
    assert(horizontal_pushwall_hits == 0);
    assert(horizontal_door_hits == 0);
}

static void test_horizontal_special_tile_dispatch(void)
{
    reset_ray_state();
    set_uniform_ray_tangent(32768);
    midangle = 450;
    tilemap[10][9] = (byte)(0x80u | 5u);
    doorposition[5] = 0;

    AsmRefresh();

    assert(horizontal_door_hits == 1);
    assert(horizontal_pushwall_hits == 0);
    assert(vertical_door_hits == 0);
    assert(vertical_pushwall_hits == 0);

    reset_ray_state();
    set_uniform_ray_tangent(32768);
    midangle = 450;
    tilemap[10][9] = (byte)(0xc0u | 5u);
    pwallpos = 0;

    AsmRefresh();

    assert(horizontal_pushwall_hits == 1);
    assert(horizontal_door_hits == 0);
    assert(vertical_pushwall_hits == 0);
    assert(vertical_door_hits == 0);
}

static void test_open_door_allows_ray_to_reach_next_wall(void)
{
    reset_ray_state();
    set_uniform_ray_tangent(65536);
    midangle = 450;

    tilemap[11][10] = (byte)(0x80u | 5u);
    doorposition[5] = 0xffffu;
    tilemap[12][10] = 1;

    AsmRefresh();

    assert(vertical_door_hits == 0);
    assert(horizontal_door_hits == 0);
    assert(vertical_pushwall_hits == 0);
    assert(horizontal_pushwall_hits == 0);
    assert(vertical_hits + horizontal_hits == 1);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof(finetangent) / sizeof(finetangent[0]); ++i) {
        finetangent[i] = 65536;
    }

    test_four_quadrants_hit_expected_vertical_wall();
    test_ray_stops_at_map_boundary();
    test_vertical_special_tile_dispatch();
    test_horizontal_special_tile_dispatch();
    test_open_door_allows_ray_to_reach_next_wall();

    puts("native raycaster tests passed");
    return 0;
}
