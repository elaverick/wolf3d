/*
 * Portable C replacement for the original 16-bit AsmRefresh ray traversal.
 *
 * The order of vertical and horizontal grid crossings, fixed-point stepping,
 * door-plane midpoint checks and push-wall offsets follow WL_DR_A.ASM. The
 * wall hit routines remain in WL_DRAW.C so texture selection and post batching
 * stay shared with the original engine.
 */

#include "WL_DEF.H"

#include <stdint.h>

extern int midangle;
extern int focaltx;
extern int focalty;

extern unsigned xpartialup;
extern unsigned ypartialup;
extern unsigned xpartialdown;
extern unsigned ypartialdown;

extern unsigned tilehit;
extern unsigned pixx;
extern int xtile;
extern int ytile;
extern int xtilestep;
extern int ytilestep;
extern fixed xintercept;
extern fixed yintercept;
extern fixed xstep;
extern fixed ystep;

extern void HitHorizWall(void);
extern void HitVertWall(void);
extern void HitHorizDoor(void);
extern void HitVertDoor(void);
extern void HitHorizPWall(void);
extern void HitVertPWall(void);

static fixed multiply_fixed_by_fraction(fixed value, unsigned fraction)
{
    return (fixed)(((int64_t)value * (int64_t)fraction) / 65536);
}

static int fixed_tile(fixed value)
{
    int64_t divisor = (int64_t)1 << TILESHIFT;
    int64_t quotient = (int64_t)value / divisor;
    int64_t remainder = (int64_t)value % divisor;

    if (remainder < 0) {
        --quotient;
    }

    return (int)quotient;
}

static int wrap_fine_angle(int value)
{
    value %= FINEANGLES;
    if (value < 0) {
        value += FINEANGLES;
    }
    return value;
}

static bool tile_coordinates_valid(int x, int y)
{
    return x >= 0 && x < MAPSIZE && y >= 0 && y < MAPSIZE;
}

static bool crossing_is_vertical(int y_coordinate)
{
    if (ytilestep < 0) {
        return y_coordinate > ytile;
    }
    return y_coordinate < ytile;
}

static bool crossing_is_horizontal(int x_coordinate)
{
    if (xtilestep < 0) {
        return x_coordinate > xtile;
    }
    return x_coordinate < xtile;
}

static bool trace_vertical_tile(void)
{
    int y_coordinate = fixed_tile(yintercept);

    if (!tile_coordinates_valid(xtile, y_coordinate)) {
        tilehit = 0;
        return false;
    }

    tilehit = tilemap[xtile][y_coordinate];
    if (tilehit != 0) {
        ytile = y_coordinate;
        xintercept = (fixed)((int64_t)xtile * TILEGLOBAL);
        return true;
    }

    spotvis[xtile][y_coordinate] = 1;
    xtile += xtilestep;
    yintercept += ystep;
    return true;
}

static bool trace_horizontal_tile(void)
{
    int x_coordinate = fixed_tile(xintercept);

    if (!tile_coordinates_valid(x_coordinate, ytile)) {
        tilehit = 0;
        return false;
    }

    tilehit = tilemap[x_coordinate][ytile];
    if (tilehit != 0) {
        xtile = x_coordinate;
        yintercept = (fixed)((int64_t)ytile * TILEGLOBAL);
        return true;
    }

    spotvis[x_coordinate][ytile] = 1;
    ytile += ytilestep;
    xintercept += xstep;
    return true;
}

static bool process_vertical_special(void)
{
    unsigned door_number;
    fixed midpoint;

    if ((tilehit & 0x40u) != 0) {
        fixed partial_step;

        partial_step = (fixed)(multiply_fixed_by_fraction(
            ystep, 0x8000u));
        midpoint = yintercept + partial_step;

        door_number = tilehit & 0x7fu;
        if (fixed_tile(midpoint) != fixed_tile(yintercept) ||
            (unsigned)(midpoint & 0xffff) < doorposition[door_number]) {
            xtile += xtilestep;
            yintercept += ystep;
            return false;
        }

        yintercept = midpoint;
        xintercept =
            (fixed)((int64_t)xtile * TILEGLOBAL + 0x8000);
        HitVertDoor();
        return true;
    }

    {
        fixed partial_step = (fixed)(
            ((int64_t)ystep * (int64_t)pwallpos) / 64);

        midpoint = yintercept + partial_step;
    }

    if (fixed_tile(midpoint) != fixed_tile(yintercept)) {
        xtile += xtilestep;
        yintercept += ystep;
        return false;
    }

    yintercept = midpoint;
    xintercept = (fixed)((int64_t)xtile * TILEGLOBAL);
    HitVertPWall();
    return true;
}

static bool process_horizontal_special(void)
{
    unsigned door_number;
    fixed midpoint;

    if ((tilehit & 0x40u) != 0) {
        fixed partial_step = (fixed)(
            multiply_fixed_by_fraction(xstep, 0x8000u));

        midpoint = xintercept + partial_step;
        door_number = tilehit & 0x7fu;

        if (fixed_tile(midpoint) != fixed_tile(xintercept) ||
            (unsigned)(midpoint & 0xffff) < doorposition[door_number]) {
            ytile += ytilestep;
            xintercept += xstep;
            return false;
        }

        xintercept = midpoint;
        yintercept =
            (fixed)((int64_t)ytile * TILEGLOBAL + 0x8000);
        HitHorizDoor();
        return true;
    }

    {
        fixed partial_step = (fixed)(
            ((int64_t)xstep * (int64_t)pwallpos) / 64);

        midpoint = xintercept + partial_step;
    }

    if (fixed_tile(midpoint) != fixed_tile(xintercept)) {
        ytile += ytilestep;
        xintercept += xstep;
        return false;
    }

    xintercept = midpoint;
    yintercept = (fixed)((int64_t)ytile * TILEGLOBAL);
    HitHorizPWall();
    return true;
}

void AsmRefresh(void)
{
    pixx = 0;

    for (pixx = 0; pixx < (unsigned)viewwidth; ++pixx) {
        int ray_angle = wrap_fine_angle(midangle + pixelangle[pixx]);
        unsigned x_partial;
        unsigned y_partial;
        int tangent_index;

        if (ray_angle < DEG90) {
            xtilestep = 1;
            ytilestep = -1;
            xstep = finetangent[DEG90 - 1 - ray_angle];
            ystep = -finetangent[ray_angle];
            x_partial = xpartialup;
            y_partial = ypartialdown;
        } else if (ray_angle < DEG180) {
            xtilestep = -1;
            ytilestep = -1;
            tangent_index = ray_angle - DEG90;
            xstep = -finetangent[tangent_index];
            ystep = -finetangent[DEG180 - 1 - ray_angle];
            x_partial = xpartialdown;
            y_partial = ypartialdown;
        } else if (ray_angle < DEG270) {
            xtilestep = -1;
            ytilestep = 1;
            xstep = -finetangent[DEG270 - 1 - ray_angle];
            ystep = finetangent[ray_angle - DEG180];
            x_partial = xpartialdown;
            y_partial = ypartialup;
        } else {
            xtilestep = 1;
            ytilestep = 1;
            xstep = finetangent[ray_angle - DEG270];
            ystep = finetangent[DEG360 - 1 - ray_angle];
            x_partial = xpartialup;
            y_partial = ypartialup;
        }

        yintercept =
            viewy + multiply_fixed_by_fraction(ystep, x_partial);
        xintercept =
            viewx + multiply_fixed_by_fraction(xstep, y_partial);

        xtile = focaltx + xtilestep;
        ytile = focalty + ytilestep;

        for (;;) {
            int y_coordinate = fixed_tile(yintercept);
            int x_coordinate = fixed_tile(xintercept);

            if (crossing_is_vertical(y_coordinate) ||
                !crossing_is_horizontal(x_coordinate)) {
                if (!trace_vertical_tile()) {
                    break;
                }

                if (tilehit == 0) {
                    continue;
                }

                if ((tilehit & 0x80u) == 0) {
                    HitVertWall();
                } else if (!process_vertical_special()) {
                    continue;
                }
                break;
            }

            if (!trace_horizontal_tile()) {
                break;
            }

            if (tilehit == 0) {
                continue;
            }

            if ((tilehit & 0x80u) == 0) {
                HitHorizWall();
            } else if (!process_horizontal_special()) {
                continue;
            }
            break;
        }
    }
}
