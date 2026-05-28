#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE      16
#define RENDER_DISTANCE 6
#define WORLD_SEED      42069

/* ── Permutation table ── */
static int P[512];
void init_perm(int seed) {
    srand(seed);
    for (int i=0;i<256;i++) P[i]=i;
    for (int i=255;i>0;i--) { int j=rand()%(i+1); int t=P[i]; P[i]=P[j]; P[j]=t; }
    for (int i=0;i<256;i++) P[i+256]=P[i];
}

static float fade(float t){ return t*t*t*(t*(t*6-15)+10); }
static float lrp(float a,float b,float t){ return a+t*(b-a); }
static float grd(int h,float x,float z){
    switch(h&3){
        case 0: return  x+z;
        case 1: return -x+z;
        case 2: return  x-z;
        default:return -x-z;
    }
}

float perlin2(float x, float z) {
    int xi=(int)floorf(x)&255, zi=(int)floorf(z)&255;
    float xf=x-floorf(x), zf=z-floorf(z);
    float u=fade(xf), w=fade(zf);
    int aa=P[P[xi  ]+zi  ], ab=P[P[xi  ]+zi+1];
    int ba=P[P[xi+1]+zi  ], bb=P[P[xi+1]+zi+1];
    float r=lrp(lrp(grd(aa,xf,zf),grd(ba,xf-1,zf),u),
                lrp(grd(ab,xf,zf-1),grd(bb,xf-1,zf-1),u),w);
    return (r+1.0f)*0.5f;
}

float fbm(float x, float z, int octaves) {
    float v=0,a=1,f=1,max=0;
    for(int i=0;i<octaves;i++){ v+=perlin2(x*f,z*f)*a; max+=a; a*=0.5f; f*=2.0f; }
    return v/max;
}

float get_height(float wx, float wz) {
    float continent = fbm(wx*0.0008f, wz*0.0008f, 4);
    float mountain  = fbm(wx*0.003f,  wz*0.003f,  6);
    float detail    = fbm(wx*0.02f,   wz*0.02f,   3);

    float h = 0;
    if (continent < 0.4f) {
        /* Ocean floor */
        h = continent * 15.0f;
    } else if (continent < 0.55f) {
        /* Coastal flatlands */
        h = 15.0f + (continent - 0.4f) * 40.0f;
    } else {
        /* Inland with mountains */
        float inland = (continent - 0.55f) / 0.45f;
        h = 21.0f + inland * mountain * mountain * 120.0f;
    }
    h += detail * 3.0f;
    return h < 5.0f ? 5.0f : h;
}

int is_path(float wx, float wz, float h) {
    if (h < 16.0f || h > 35.0f) return 0;
    float p = fbm(wx*0.015f+500, wz*0.015f+500, 2);
    return (p > 0.47f && p < 0.53f);
}

void biome_color(float h, float wx, float wz,
                 unsigned char *r, unsigned char *g, unsigned char *b) {
    if (is_path(wx, wz, h)) { *r=180; *g=20; *b=20; return; }
    if      (h < 7.0f)  { *r=20;  *g=40;  *b=140; }  /* deep ocean */
    else if (h < 11.0f) { *r=35;  *g=65;  *b=175; }  /* shallow */
    else if (h < 14.0f) { *r=30;  *g=100; *b=190; }  /* coast water */
    else if (h < 16.0f) { *r=210; *g=195; *b=110; }  /* sand */
    else if (h < 20.0f) { *r=180; *g=175; *b=90;  }  /* dry grass */
    else if (h < 32.0f) { *r=65;  *g=145; *b=50;  }  /* grass */
    else if (h < 50.0f) { *r=45;  *g=105; *b=35;  }  /* forest */
    else if (h < 75.0f) { *r=90;  *g=80;  *b=70;  }  /* rock */
    else if (h < 95.0f) { *r=65;  *g=60;  *b=55;  }  /* dark rock */
    else                { *r=235; *g=240; *b=245; }  /* snow */
}

/* ── Chunk ── */
typedef struct {
    int cx, cz, loaded;
    Model model;
    /* Trees */
    Vector3 tree_pos[48];
    float   tree_h[48];
    int     tree_count;
} Chunk;

void gen_chunk(Chunk *c, int cx, int cz) {
    c->cx=cx; c->cz=cz; c->loaded=1; c->tree_count=0;
    int quads=(CHUNK_SIZE-1)*(CHUNK_SIZE-1);
    int vcount=quads*6;
    float *verts  = malloc(vcount*3*sizeof(float));
    float *norms  = malloc(vcount*3*sizeof(float));
    float *uvs    = malloc(vcount*2*sizeof(float));
    unsigned char *cols = malloc(vcount*4);
    int vi=0,ni=0,ui=0,ci=0;

    for(int x=0;x<CHUNK_SIZE-1;x++) for(int z=0;z<CHUNK_SIZE-1;z++) {
        float wx=cx*CHUNK_SIZE+x, wz=cz*CHUNK_SIZE+z;
        float h00=get_height(wx,   wz  );
        float h10=get_height(wx+1, wz  );
        float h01=get_height(wx,   wz+1);
        float h11=get_height(wx+1, wz+1);
        unsigned char r,g,b;
        biome_color(h00,wx,wz,&r,&g,&b);

        /* smooth normal from neighbour heights */
        float dzdx=(h10-h00), dzdz=(h01-h00);
        float nx=-dzdx, ny=1.0f, nz=-dzdz;
        float nl=sqrtf(nx*nx+ny*ny+nz*nz);
        nx/=nl; ny/=nl; nz/=nl;

        /* tri 1 */
        float tv1[3][3]={{wx,h00,wz},{wx+1,h10,wz},{wx,h01,wz+1}};
        for(int k=0;k<3;k++){
            verts[vi++]=tv1[k][0]; verts[vi++]=tv1[k][1]; verts[vi++]=tv1[k][2];
            norms[ni++]=nx; norms[ni++]=ny; norms[ni++]=nz;
            uvs[ui++]=0; uvs[ui++]=0;
            cols[ci++]=r; cols[ci++]=g; cols[ci++]=b; cols[ci++]=255;
        }
        /* tri 2 */
        float tv2[3][3]={{wx+1,h10,wz},{wx+1,h11,wz+1},{wx,h01,wz+1}};
        for(int k=0;k<3;k++){
            verts[vi++]=tv2[k][0]; verts[vi++]=tv2[k][1]; verts[vi++]=tv2[k][2];
            norms[ni++]=nx; norms[ni++]=ny; norms[ni++]=nz;
            uvs[ui++]=0; uvs[ui++]=0;
            cols[ci++]=r; cols[ci++]=g; cols[ci++]=b; cols[ci++]=255;
        }

        /* trees */
        if(h00>20.0f && h00<50.0f && c->tree_count<48){
            float tp=perlin2(wx*0.4f+77, wz*0.4f+77);
            if(tp>0.82f){
                c->tree_pos[c->tree_count]=(Vector3){wx,h00,wz};
                c->tree_h[c->tree_count]=4.5f+tp*5.0f;
                c->tree_count++;
            }
        }
    }

    Mesh m={0};
    m.vertexCount=vcount; m.triangleCount=vcount/3;
    m.vertices=verts; m.normals=norms; m.texcoords=uvs; m.colors=cols;
    UploadMesh(&m,false);
    c->model=LoadModelFromMesh(m);
}

/* ── Player ── */
typedef struct {
    Vector3 pos;
    float yaw,pitch,speed,health,stamina;
} Player;

void save_game(Player *p){ FILE *f=fopen("save.hrc","wb"); if(f){fwrite(p,sizeof(*p),1,f);fclose(f);} }
int  load_game(Player *p){ FILE *f=fopen("save.hrc","rb"); if(f){fread(p,sizeof(*p),1,f);fclose(f);return 1;} return 0; }

/* ── Portal ── */
typedef struct { Vector3 pos; Color col; char label[32]; } Portal;

int main(void) {
    init_perm(WORLD_SEED);
    InitWindow(1280,720,"Havoks Reach");
    SetTargetFPS(60);
    DisableCursor();

    Camera3D cam={0};
    cam.up=(Vector3){0,1,0};
    cam.fovy=70; cam.projection=CAMERA_PERSPECTIVE;

    Player pl={0};
    pl.speed=18; pl.health=100; pl.stamina=100;
    if(!load_game(&pl)){
        pl.pos=(Vector3){512,0,512};
        pl.pos.y=get_height(512,512)+2;
    }

    Portal portals[]={
        {{520,0,512},{255,50,50,255},"Realm: Home"},
        {{535,0,512},{50,50,255,255},"Realm: Dungeon"},
        {{550,0,512},{50,200,50,255},"Realm: Market"},
    };
    int nportals=3;
    for(int i=0;i<nportals;i++)
        portals[i].pos.y=get_height(portals[i].pos.x,portals[i].pos.z);

    int total=RENDER_DISTANCE*2*RENDER_DISTANCE*2;
    Chunk *chunks=malloc(total*sizeof(Chunk));
    int nchunks=0;
    int pcx=(int)(pl.pos.x/CHUNK_SIZE);
    int pcz=(int)(pl.pos.z/CHUNK_SIZE);
    for(int cx=pcx-RENDER_DISTANCE;cx<pcx+RENDER_DISTANCE;cx++)
        for(int cz=pcz-RENDER_DISTANCE;cz<pcz+RENDER_DISTANCE;cz++)
            gen_chunk(&chunks[nchunks++],cx,cz);

    float lcd=0,rcd=0,atimer=0;
    int alm=0,arm=0;
    char msg[64]=""; float msg_t=0;

    while(!WindowShouldClose()){
        float dt=GetFrameTime();

        /* look */
        Vector2 md=GetMouseDelta();
        pl.yaw  -=md.x*0.003f;
        pl.pitch-=md.y*0.003f;
        if(pl.pitch> 1.4f) pl.pitch= 1.4f;
        if(pl.pitch<-1.4f) pl.pitch=-1.4f;

        Vector3 fwd={cosf(pl.pitch)*sinf(pl.yaw), sinf(pl.pitch), cosf(pl.pitch)*cosf(pl.yaw)};
        Vector3 rgt={sinf(pl.yaw-1.5707f),0,cosf(pl.yaw-1.5707f)};
        float sp=pl.speed*(IsKeyDown(KEY_LEFT_SHIFT)?2.5f:1.0f);

        if(IsKeyDown(KEY_W)){pl.pos.x+=fwd.x*sp*dt; pl.pos.z+=fwd.z*sp*dt;}
        if(IsKeyDown(KEY_S)){pl.pos.x-=fwd.x*sp*dt; pl.pos.z-=fwd.z*sp*dt;}
        if(IsKeyDown(KEY_A)){pl.pos.x-=rgt.x*sp*dt; pl.pos.z-=rgt.z*sp*dt;}
        if(IsKeyDown(KEY_D)){pl.pos.x+=rgt.x*sp*dt; pl.pos.z+=rgt.z*sp*dt;}
        if(IsKeyDown(KEY_SPACE))        pl.pos.y+=sp*dt;
        if(IsKeyDown(KEY_LEFT_CONTROL)) pl.pos.y-=sp*dt;

        float gnd=get_height(pl.pos.x,pl.pos.z)+2.0f;
        if(pl.pos.y<gnd) pl.pos.y=gnd;

        cam.position=pl.pos;
        cam.target=(Vector3){pl.pos.x+fwd.x,pl.pos.y+fwd.y,pl.pos.z+fwd.z};

        /* attacks */
        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&lcd<=0){alm=1;atimer=0.3f;lcd=0.5f;}
        if(IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)&&rcd<=0){arm=1;atimer=0.5f;rcd=0.8f;}
        if(lcd>0)lcd-=dt; if(rcd>0)rcd-=dt;
        if(atimer>0)atimer-=dt; else{alm=0;arm=0;}

        /* portals */
        if(IsKeyPressed(KEY_E)){
            for(int i=0;i<nportals;i++){
                Vector3 pp={portals[i].pos.x,pl.pos.y,portals[i].pos.z};
                if(Vector3Distance(pl.pos,pp)<5.0f){
                    snprintf(msg,64,"Entering %s...",portals[i].label);
                    msg_t=3.0f;
                }
            }
        }
        if(IsKeyPressed(KEY_F5)){save_game(&pl);snprintf(msg,64,"Saved!");msg_t=2.0f;}
        if(msg_t>0)msg_t-=dt;
        if(IsKeyPressed(KEY_ESCAPE))break;

        BeginDrawing();
        ClearBackground((Color){100,140,200,255});

        BeginMode3D(cam);

        for(int i=0;i<nchunks;i++){
            DrawModel(chunks[i].model,(Vector3){0,0,0},1,WHITE);
            for(int t=0;t<chunks[i].tree_count;t++){
                Vector3 tp=chunks[i].tree_pos[t];
                float   th=chunks[i].tree_h[t];
                DrawCylinder(tp,0.25f,0.25f,th,6,BROWN);
                DrawSphere((Vector3){tp.x,tp.y+th,tp.z},th*0.45f,(Color){30,110,25,255});
            }
        }

        /* portals */
        for(int i=0;i<nportals;i++){
            Vector3 pp=portals[i].pos;
            DrawCube((Vector3){pp.x,pp.y+2.5f,pp.z},2,5,2,portals[i].col);
            DrawCubeWires((Vector3){pp.x,pp.y+2.5f,pp.z},2.1f,5.1f,2.1f,WHITE);
        }

        if(alm) DrawSphere((Vector3){pl.pos.x+fwd.x*2,pl.pos.y+fwd.y*2,pl.pos.z+fwd.z*2},0.3f,RED);
        if(arm) DrawSphere((Vector3){pl.pos.x+fwd.x*3,pl.pos.y+fwd.y*3,pl.pos.z+fwd.z*3},0.5f,ORANGE);

        EndMode3D();

        /* HUD */
        DrawText("HAVOKS REACH",10,10,28,WHITE);
        DrawText(TextFormat("FPS: %d",GetFPS()),10,45,18,YELLOW);
        DrawText(TextFormat("XYZ: %.0f %.0f %.0f",pl.pos.x,pl.pos.y,pl.pos.z),10,68,16,WHITE);
        DrawText("WASD Move | SHIFT Sprint | SPACE Up | CTRL Down | F5 Save | E Interact | ESC Quit",10,700,13,LIGHTGRAY);

        DrawRectangle(10,635,204,20,DARKGRAY);
        DrawRectangle(11,636,(int)(pl.health*2),18,RED);
        DrawText(TextFormat("HP %.0f",pl.health),14,637,14,WHITE);

        DrawRectangle(10,658,204,20,DARKGRAY);
        DrawRectangle(11,659,(int)(pl.stamina*2),18,GREEN);
        DrawText(TextFormat("ST %.0f",pl.stamina),14,660,14,WHITE);

        if(msg_t>0) DrawText(msg,480,340,22,YELLOW);
        if(alm) DrawText("LIGHT ATTACK",500,370,20,RED);
        if(arm) DrawText("HEAVY ATTACK",500,370,20,ORANGE);

        for(int i=0;i<nportals;i++){
            Vector3 pp={portals[i].pos.x,pl.pos.y,portals[i].pos.z};
            if(Vector3Distance(pl.pos,pp)<8.0f)
                DrawText(TextFormat("[E] Enter %s",portals[i].label),430,400,20,YELLOW);
        }

        EndDrawing();
    }

    for(int i=0;i<nchunks;i++) UnloadModel(chunks[i].model);
    free(chunks);
    CloseWindow();
    return 0;
}