#include "advancedConsole.h"
#include "colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <math.h>

struct tile;
struct tileState;
struct tileComplete;

typedef unsigned char tile_id;

#define OPENWORLD
#define TILE_COUNT 12
#define NORTH_F x,y-1
#define EAST_F x+1,y
#define SOUTH_F x,y+1
#define WEST_F x-1,y

int tileMapHeight = 128;
int tileMapWidth = 128;
const int chunkSize = 16;
int chunkCountX = 8;
int chunkCountY = 8;

int textureSize = 8;
double scale = 4.0f;
int selectorTileId = 0;
double viewX;
double viewY;
int selectorX;
int selectorY;
bool infoMode = false;
double viewBoxWidth;
double viewBoxHeight;

//debug
float m_offsetx = 0.0f;
float m_offsety = 0.0f;
float m_posx = 0.0f;
float m_posy = 0.0f;
int d_drawCallCount;
int d_pixelDrawn;

//player
double playerX;
double playerY;
double playerXvelocity;
double playerYvelocity;
double playerXacceleration;
double playerYacceleration;
bool grounded;
double gravity = -9.8f;

unsigned char *texture;
int textureHeight;
int textureWidth;
int bpp;

struct ch_co_t {
	wchar_t ch;
	color_t co;
	color_t a;
} *texturechco;

struct pixel {
	pixel() {
		r = 0;
		g = 0;
		b = 0;
		a = 255;
	}
	pixel(color_t r, color_t g, color_t b) {
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = 255;
	}
	color_t r,g,b,a;
};

pixel sampleImage(float x, float y) { 
	int imX = x * textureWidth;
	int imY = y * textureHeight;
	
	pixel pix;
	pix.r = texture[imY * (textureWidth * bpp) + (imX * bpp) + 0];
	pix.g = texture[imY * (textureWidth * bpp) + (imX * bpp) + 1];
	pix.b = texture[imY * (textureWidth * bpp) + (imX * bpp) + 2];
	
	if (bpp == 4)
		pix.a = texture[imY * (textureWidth * bpp) + (imX * bpp) + 3];
	
	return pix;
}

ch_co_t sampleImageCHCO(float x, float y) {
	int imX = x * textureWidth;
	int imY = y * textureHeight;
	ch_co_t chco = texturechco[imY * textureWidth + imX];
	return chco;
}

struct tiles {
	static tile *tileRegistry[TILE_COUNT];
	static int id;
	
	static void add(tile *tile);
	static tile *get(tile_id id);
	
	static tile *AIR;
	static tile *STONE;
	static tile *DIRT;
};

int tiles::id = 0;
tile *tiles::tileRegistry[TILE_COUNT];

struct world_t;
world_t *world;
struct chunk_t;

struct world_t {	
	chunk_t *chunks;

	tileState *getState(int x, int y);
	
	tileComplete getComplete(int x, int y);
	
	tile *getTile(tileState *state);
	
	tileComplete place(int x, int y, tileState tile);
};


struct tileComplete {
	tile *parent;
	tileState *state;
	int tileX;
	int tileY;
};

struct tileState {
	tileState() { id = 0; data.b = 0; }
	tile_id id;
	union {
		unsigned char a[4];
		unsigned int b;
	} data;
	void setConnection(int direction, int index = 0) {
		data.a[index] |= direction;
	}
	bool hasConnection(int direction, int index = 0) {
		return (data.a[index] & direction);
	}
};

struct chunk_t {
	tileState tileMap[chunkSize][chunkSize];
	
	void generate();
	
	int originX, originY;
	
	bool generated;
};

struct tile {
	tile_id id;
	tileState defaultState;
	unsigned char textureAtlas[4];
	
	tile() {
		id = 0;
		
		textureAtlas[0] = 0;
		textureAtlas[1] = 0;
		textureAtlas[2] = 1;
		textureAtlas[3] = 1;
		
		defaultState = getDefaultState();
	}
	
	tile(int t0, int t1, int t2, int t3, bool skip = false) {
		if (!skip) 
			tiles::add(this);
				
		setAtlas(t0,t1,t2,t3);
		
		defaultState = getDefaultState();
	}
	
	void setAtlas(int a, int b, int c, int d) {
		textureAtlas[0] = a;
		textureAtlas[1] = b;
		textureAtlas[2] = c;
		textureAtlas[3] = d;
	}
	
	virtual tileState getDefaultState() {
		tileState state;
		state.id = id;
		return state;
	}
	
	virtual void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) {
		d_drawCallCount++;
		if (offsetx >= adv::width || offsety >= adv::height || offsetx + sizex < 0 || offsety + sizey < 0)
			return;
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				ch_co_t chco = sampleImageCHCO(xf, yf);
				if (chco.a < 255)
					continue;
				d_pixelDrawn++;
				adv::write(offsetx + x, offsety + y, chco.ch, chco.co);
			}
		}
	}
	
	virtual void onCreate(tileComplete *tc, int x, int y) {}
	
	virtual void onUpdate(tileComplete *tc, int x, int y) {}
	
	virtual void onDestroy(tileComplete *tc, int x, int y) {}
};


struct chunkServer {
	
};

/*

chunking thoughts

chunkConsumer has chunks^2 slots
chunkConsumer has a quick map for coordinates, slots are taken up on an as need basis
a chunk slot can only be released when the chunkserver determines it can be released

chunkserver manages loading/unloading/generating only based upon what chunkconsumer requests
if chunkconsumer requests 5x5 chunks it will send 5x5 chunks back with references to chunks that are still generating
chunkserver does not store in lieu of the chunkconsumer
chunkserver will store references to the chunks in chunkconsumers

no this will not work




*/

struct chunkConsumer {
	int x, int y;
	int chunks;
	
	void setPosition(int x, int y) {
		this->x = x;
		this->y = y;
	}
	
	void setDistance(int chunks) {
		this->chunks = chunks;
	}
};



struct tileable : public tile {
	tileable() {}
	tileable(int t0, int t1, int t2, int t3, bool skip = false) 
		:tile(t0,t1,t2,t3,skip) {
			//connectionAtlas = {1,1,2,2};
			connectionAtlas[0]  = 1;
			connectionAtlas[1]  = 1;
			connectionAtlas[2]  = 2;
			connectionAtlas[3]  = 2;
			svarSize = 0.25f;
	}
	tileable(int t0, int t1, int t2, int t3, int t4, int t5, int t6, int t7, float svarSize, bool skip = false) 
		:tile(t0,t1,t2,t3,skip) {
			//connectionAtlas = {t4,t5,t6,t7};
			connectionAtlas[0]  = t4;
			connectionAtlas[1]  = t5;
			connectionAtlas[2]  = t6;
			connectionAtlas[3]  = t7;
			this->svarSize = svarSize;
	}
	
	int connectionAtlas[4];
	float svarSize;
	
	virtual void connectToNeighbors(tileComplete *tc, int x, int y) {
		tileState *tp = tc->state;
		unsigned char old = tp->data.a[0];
		tp->setConnection(0);
		tp->data.a[0] &= tp->data.a[0] ^ 0x0f;
		tileState *neighbors[4];
		neighbors[0] = world->getState(x,y-1);//NORTH
		neighbors[1] = world->getState(x+1,y);//EAST
		neighbors[2] = world->getState(x,y+1);//SOUTH
		neighbors[3] = world->getState(x-1,y);//WEST
		for (int i = 0; i < 4; i++) {
			//if (neighbors[i]->id == tp->id) {
			if (neighbors[i]->id != tiles::AIR->id) {
				tp->setConnection(1 << i);
			}
		}
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		d_drawCallCount++;
		//NORTH
		float svars[][4] = {
			{0, sizex, 0, (sizey*svarSize)},
			{sizex - (sizex * svarSize), sizex, 0, sizey},
			{0, sizex, sizey - (sizey * svarSize), sizey},
			{0, sizex * svarSize, 0, sizey}
		};
				
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {		
				if (!adv::bound(offsetx + x, offsety + y))
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				ch_co_t chco = sampleImageCHCO(xf,yf);
				for (int i = 0; i < 4; i++) {
					if (!connectingCondition(tc, 1 << i)) {
						float xfto = ((connectionAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
						float yfto = ((connectionAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;			
						if ((svars[i][0] <= x && svars[i][1] > x && svars[i][2] <= y && svars[i][3] > y)) {
							ch_co_t chco2 = sampleImageCHCO(xfto, yfto);
							if (chco2.a == 0) {
								chco.a = 0;
							} else
							if (chco2.a == 255) {
								chco = chco2;
							}		
						}
					}
				}
				if (chco.a  < 255)
					continue;
				d_pixelDrawn++;
				adv::write(offsetx + x, offsety + y, chco.ch, chco.co);
			}
		}		
	}
	
	virtual bool connectingCondition(tileComplete *tc, int direction) {
		return tc->state->hasConnection(direction);
	}
	
	void onCreate(tileComplete *tc, int x, int y) override {
		connectToNeighbors(tc,x,y);
	}
	
	void onUpdate(tileComplete *tc, int x, int y) override {
		connectToNeighbors(tc,x,y);
	}
};

void tiles::add(tile *tile) {
	tile->id = id++;
	if (tile->id < TILE_COUNT)
		tiles::tileRegistry[tile->id] = tile;
}

tile *tiles::get(tile_id id) {
	if (id < TILE_COUNT)
		return tiles::tileRegistry[id];
	else
		return tiles::AIR;
}

tileState *world_t::getState(int x, int y) {
		if (x >= tileMapWidth || x < 0 || y >= tileMapWidth || y < 0)
			return &tiles::AIR->defaultState;
		//return &tileMap[y * tileMapWidth + x];
		int coriginx = x / chunkSize;
		int coriginy = y / chunkSize;
		return &chunks[coriginy * chunkCountX + coriginx].tileMap[x - (coriginx * chunkSize)][y - (coriginy * chunkSize)];
}

tileComplete world_t::getComplete(int x, int y) {
		tileComplete tc;
		tc.state = getState(x,y);
		tc.parent =getTile(tc.state);
		tc.tileX = x;
		tc.tileY = y;
		return tc;	
}

tile *world_t::getTile(tileState *state) {
	return tiles::get(state->id);
}

tileComplete world_t::place(int x, int y, tileState tile) {
	tileComplete stale = getComplete(x,y);
	stale.parent->onDestroy(&stale, x, y);
	*getState(x,y) = tile;
	tileComplete newtile = getComplete(x,y);
	newtile.parent->onCreate(&newtile, x, y);
	
	tileComplete neighbor;
	neighbor = getComplete(NORTH_F);
	neighbor.parent->onUpdate(&neighbor, NORTH_F);
	neighbor = getComplete(EAST_F);
	neighbor.parent->onUpdate(&neighbor, EAST_F);
	neighbor = getComplete(SOUTH_F);
	neighbor.parent->onUpdate(&neighbor, SOUTH_F);
	neighbor = getComplete(WEST_F);
	neighbor.parent->onUpdate(&neighbor, WEST_F);		
	
	return newtile;
}

tile *tiles::AIR = new tile(0,0,1,1);
tile *air = tiles::AIR;
tile *stone = new tileable(0,1,1,2);
tile *dirt = new tileable(1,0,2,1, 3,1,4,2,0.25f);
tile *stonebricks = new tileable(2,0,3,1,2,1,3,2, 0.125f);
tile *gold = new tileable(3,0,4,1);
tile *wood_horizontal = new tile(5,0,6,1);
tile *wood_vertical = new tile(5,1,6,2);
tile *leaves = new tileable(6,1,7,2,7,1,8,2,0.25f);
tile *glass = new tileable(6,0,7,1,2,1,3,2,0.125f);
tile *nothing = new tile();

//Perlin Noise

#ifdef OPENWORLD
class perlin {
	public:
		static float getPerlin(float x, float y);
		static float getPerlin(float x, float y, int octaves, float persistence);
		static float getNormal(float omax, float omin, float max, float min, float value);
		static float getNormalNoise(float x, float z);
		static float cosine_interpolate(float a, float b, float x);
		static float smooth_noise_2D(float x, float y);
		static float interpolated_noise(float x, float y);
		static float noise(int x, int y);
		
		static float octaves;
		static float persistence;
		static float lacunarity;
};

#define PI 3.1415927

float perlin::noise(int x, int y) {
    int n = x + y * 57;
    n = (n<<13) ^ n;
    return (1.0 - ( (n * ((n * n * 15731) + 789221) +  1376312589) & 0x7fffffff) / 1073741824.0);
}

float perlin::cosine_interpolate(float a, float b, float x) {
    float ft = x * PI;
    float f = (1 - cos(ft)) * 0.5;
    float result =  a*(1-f) + b*f;
    return result;
}

float perlin::smooth_noise_2D(float x, float y) {  
    float corners = ( noise(x-1, y-1)+noise(x+1, y-1)+noise(x-1, y+1)+noise(x+1, y+1) ) / 16;
    float sides   = ( noise(x-1, y)  +noise(x+1, y)  +noise(x, y-1)  +noise(x, y+1) ) /  8;
    float center  =  noise(x, y) / 4;

    return corners + sides + center;
}

float perlin::interpolated_noise(float x, float y) {
    int x_whole = (int) x;
    float x_frac = x - x_whole;

    int y_whole = (int) y;
    float y_frac = y - y_whole;

    float v1 = smooth_noise_2D(x_whole, y_whole); 
    float v2 = smooth_noise_2D(x_whole, y_whole+1); 
    float v3 = smooth_noise_2D(x_whole+1, y_whole); 
    float v4 = smooth_noise_2D(x_whole+1, y_whole+1); 

    float i1 = cosine_interpolate(v1,v3,x_frac);
    float i2 = cosine_interpolate(v2,v4,x_frac);

    return cosine_interpolate(i1, i2, y_frac);
}

float perlin::octaves = 8.0f;
float perlin::persistence = 0.5f;
float perlin::lacunarity = 1.0f;

float perlin::getPerlin(float x, float y, int octaves, float persistence) {
    float total = 0;

    for(int i=0; i<octaves-1; i++)
    {
        float frequency = pow(2,i);
        float amplitude = pow(persistence,i);
        total = total + interpolated_noise(x * frequency, y * frequency) * amplitude;
    }
    return total;
}

float perlin::getPerlin(float x, float y) {
    float total = 0;
	//x += int(1 << 20);
	//y += int(1 << 20);

    for(int i=0; i<octaves-1; i++)
    {
        float frequency = pow(2,i);
        float amplitude = pow(persistence,i);
        total = total + interpolated_noise(x * frequency, y * frequency) * amplitude;
    }
    return total;
}
 
float perlin::getNormal(float omax, float omin, float max, float min, float value) {
	return (max - min) / (omax - omin) * (value - omax) + max;
}

float perlin::getNormalNoise(float x, float z) {
	float noise = perlin::getPerlin(x,z);
	return perlin::getNormal(1,-1,1,0,noise);
}
#endif

void chunk_t::generate() {
	int ofx = 128 - (originX * chunkSize);
	int ofy = 128 - (originY * chunkSize);
	
	for (int x = 0; x < chunkSize; x++) {
		for (int y = 0; y < chunkSize; y++) {
			tileMap[x][y] = air->getDefaultState();
			
			//perlin::octaves = 2.0f;
			if (perlin::getPerlin((ofx + x) * 0.1f + 1000.0f, ((ofy + y) / 2.0f) * 0.1f + 1000.0f) < 0.15f)
			if (ofy + y < 112)
				if (float(rand() % 120) / 120.0f < 0.8f * (float(ofy + y) / 120.0f))
					tileMap[x][y] = dirt->getDefaultState();
				else
					tileMap[x][y] = stone->getDefaultState();
		}
	}
	
	//Creation update, not good enough to do texture connections
	for (int x = 0; x < chunkSize; x++) {
		for (int y = 0; y < chunkSize; y++) {
			int ofx = originX * chunkSize, ofy = originY * chunkSize;
			tileComplete cmp = world->getComplete(ofx + x, ofy + y);
			cmp.parent->onCreate(&cmp, ofx + x, ofy + y);
		}
	}
}

void init() {
	srand(time(NULL));
	
	viewX = 0;
	viewY = 0;
	selectorX = 0;
	selectorY = 0;
	selectorTileId = 1;
	scale = 8.0f;//4;
	
	playerX = -32.0f;
	playerY = 25.0f;
	playerYvelocity = 0;
	
	if (world) {
		if (world->chunks)
			delete [] world->chunks;
		delete world;
	}
	
	world = new world_t;
	//world->tileMap = new tileState[tileMapWidth * tileMapHeight];
	world->chunks = new chunk_t[chunkCountX * chunkCountY];
	
	/*
	for (int y = 0; y < tileMapHeight; y++) {
		for (int x = 0; x < tileMapWidth; x++) {
			*world->getState(x,y) = air->getDefaultState();
			
			if (rand() % 2 == 0)
				*world->getState(x,y) = stone->getDefaultState();
			if (rand() % 2 == 0)
				*world->getState(x,y) = dirt->getDefaultState();
			if (rand() % 2 == 0)
				*world->getState(x,y) = iron->getDefaultState();
		}
	}
	*/	
	
	for (int x = 0; x < chunkCountX; x++) {
		for (int y = 0; y < chunkCountY; y++) {
			chunk_t *currentChunk = &world->chunks[y * chunkCountX + x];
			currentChunk->originX = x;
			currentChunk->originY = y;
			currentChunk->generate();
		}
	}
	
	for (int x = 0; x < chunkCountX * chunkSize; x++) {
		for (int y = 0; y < chunkCountY * chunkSize; y++) {
			tileComplete cmp = world->getComplete(x,y);
			cmp.parent->onUpdate(&cmp, x, y);
		}
	}
	
	/*
	for (int y = 0; y < tileMapHeight; y++) {
		for (int x = 0; x < tileMapWidth; x++) {
			tileComplete cmp = world->getComplete(x,y);
			cmp.parent->onCreate(&cmp, x, y);
		}
	}
	*/
}

void display() {
	{		
		int width = 2 * scale;
		int height = 1 * scale;
		for (int x = 0; x < adv::width; x++) {
			for (int y = 0; y < adv::height; y++) {
				adv::write(x,y,'#', FGREEN | BBLACK);
			}
		}
	}
	
	//background
	{
		int backgroundTexture[] = { 0, 2, 4, 5 };
		//int backgroundTexture[] = { 4, 2, 8, 5 };
		for (int x = 0; x < adv::width; x++) {
			for (int y = 0; y < adv::height; y++) {
				float xf = ((backgroundTexture[0] * textureSize) + ((float(x) / adv::width) * textureSize * 4)) / textureWidth;
				float yf = ((backgroundTexture[1] * textureSize) + ((float(y) / adv::height) * textureSize * 3)) / textureHeight;				
				ch_co_t chco = sampleImageCHCO(xf, yf);
				if (chco.a < 255)
					continue;
				adv::write(x, y, chco.ch, chco.co);
			}
		}
	}
	
	//tiles
	{
		double  width = 2 * scale;
		double height = 1 * scale;
		tileComplete tc;
		
		for (int x = 0; x < tileMapWidth; x++) {
			for (int y = 0; y < tileMapHeight; y++) {
				double offsetx = x + viewX;
				double offsety = y + viewY;
				
				if (offsetx * width + width < 0 || offsety * height + height < 0 || offsetx * width + width - width > adv::width || offsety * height + height - height > adv::height)
					continue;
				
				tc = world->getComplete(x,y);
				tileState *state = tc.state;
				
				if (x == playerX && y == playerY)
					tc.parent = gold;
				
				tc.parent->draw(&tc, offsetx * width, offsety * height, width, height);
			}
		}
	}
	
	//player
	{
		int width = 2 * scale;
		int height = 1 * scale;
		int playerTexture[] = { 4, 0, 5, 2 };
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height * 2; y++) {
				float xf = ((playerTexture[0] * textureSize) + ((float(x) / width) * textureSize)) / textureWidth;
				float yf = ((playerTexture[1] * (textureSize * 2)) + ((float(y) / (height * 2)) * (textureSize * 2.0f))) / textureHeight;
				ch_co_t chco = sampleImageCHCO(xf, yf);
				if (chco.a < 255)
					continue;
				adv::write(((adv::width / 2.0f) - (width / 2.0f)) + x, ((adv::height / 2.0f) - ((height))) + y, chco.ch, chco.co);				
			}
		}
	}
	
	//hotbar
	{
		int width = 2 * 4;
		int height = 1 * 4;
		int tileCount = 7;
		for (int i = 0; i < tileCount + 1; i++) {
			tileComplete cmp;
			cmp.parent = tiles::tileRegistry[i + 1];
			tileState state = cmp.parent->getDefaultState();
			cmp.state = &state;
			tiles::tileRegistry[i + 1]->draw(&cmp, 0 + (width * i), 0 , width, height);
		}
		adv::border(0 + (width * (selectorTileId - 1)), 0, 0 + (width * (selectorTileId - 1)) + width, 0 + height, FRED|BBLACK);
	}	
	
	if (infoMode) {
		int y = 0;
		auto printVar = [&](const char *varname, float value) {
			char buf[100];
			int i = snprintf(&buf[0], 99, "%s: %f", varname, value);
			adv::write(0, y++, &buf[0]);
		};
		printVar("selectorTileId", selectorTileId);
		printVar("viewX", viewX);
		printVar("viewY", viewY);
		printVar("scale", scale);
		printVar("selectorX", selectorX);
		printVar("selectorY", selectorY);
		printVar("infoMode", infoMode ? 1.0f : 0.0f);
		printVar("m_offsetx", m_offsetx);
		printVar("m_offsety", m_offsety);
		printVar("m_posx", m_posx);
		printVar("m_posy", m_posy);
		printVar("width", 2 * scale);
		printVar("height", 1 * scale);
		printVar("cwidth", adv::width);
		printVar("cheight", adv::height);
		printVar("d_drawCallCount", d_drawCallCount);
		printVar("d_pixelDrawn", d_pixelDrawn);
		printVar("playerX", playerX);
		printVar("playerY", playerY);
		printVar("playerXacc", playerXacceleration);
		printVar("playerYacc", playerYacceleration);
		printVar("playerXvel", playerXvelocity);
		printVar("playerYvel", playerYvelocity);
		printVar("viewBoxWidth", viewBoxWidth);
		printVar("viewBoxHeight", viewBoxHeight);
		
		d_drawCallCount = 0;
		d_pixelDrawn = 0;
	}
}

int wmain() {
	colormapper_init_table();
	
	texture = stbi_load("textures.png", (int*)&textureWidth, (int*)&textureHeight, &bpp, 0);
	
	texturechco = new ch_co_t[textureWidth * textureHeight];
	
	world = nullptr;
	
	for (int x = 0; x < textureWidth; x++) {
		for (int y = 0; y < textureHeight; y++) {
			pixel pix = sampleImage(float(x) / textureWidth, float(y) / textureHeight);
	tileComplete getComplete(int x, int y);
			ch_co_t chco;
			chco.a = pix.a;
			getDitherColored(pix.r, pix.g, pix.b, &chco.ch, &chco.co);
			texturechco[int(y * textureWidth) + int(x)] = chco;
		}
	}
	
	while (!adv::ready) console::sleep(10);
	
	adv::setThreadState(false);
	//adv::setThreadSafety(false);
	
	printf("\033[?1003h\n");
	mouseinterval(1);
	mousemask(ALL_MOUSE_EVENTS, NULL);
	MEVENT event;
	
	init();
	
	int key = 0;
	
	auto tp1 = std::chrono::system_clock::now();
	auto tp2 = std::chrono::system_clock::now();
	
	while (!HASKEY(key = console::readKeyAsync(), VK_ESCAPE)) {
		adv::clear();
		
		switch (key) {
			case KEY_MOUSE:
			{
				float width = 2 * scale;
				float height = 1 * scale;
				//adv::width / width;
				//float offsetx = m_offsetx = fabs(viewX) / width;
				//float offsety = m_offsety = fabs(viewY) / height;
				float offsetx = m_offsetx = -(viewX);
				float offsety = m_offsety = -(viewY);
				m_posx = offsetx + (event.x / float(width));
				m_posy = offsety + (event.y / float(height));
				if (getmouse(&event) == OK) {
					if (event.x < 8 * 8 && event.y < 4) {
						if (event.bstate & BUTTON1_RELEASED) {
							selectorTileId = ((float(event.x) / (8.0f)) + 1);
						}
					}
					if (event.bstate & BUTTON1_RELEASED)
						world->place(int(m_posx), int(m_posy), tiles::AIR->getDefaultState());
					if (event.bstate & BUTTON3_RELEASED)
						world->place(int(m_posx), int(m_posy), tiles::get(selectorTileId)->getDefaultState());
				}
			}
				break;				
			case VK_UP:
			case '1':
				selectorTileId--;
				if (selectorTileId < 1)
					selectorTileId = 8;
				break;
			case VK_DOWN:
			case '2':
				selectorTileId++;
				if (selectorTileId > 8)
					selectorTileId = 1;
				break;
			case ' ':
					//if (grounded)
						playerYvelocity = 12.5;
				break;
			case 'w':
					playerY++;
				break;
			case 'a':
					playerX++;
				break;
			case 's':
					playerY--;
				break;
			case 'd':
					playerX--;	
				break;
			case 'l':
					playerX-=0.25f;
					break;
			case ',':
			//case 'z':
				scale+=0.25f;
				break;
			case '.':
			//case 'x':
				if (scale > 0.5f)
					scale-=0.25f;
				break;
			case '/':
				scale = 4;
				break;
			case '0':
				init();
				break;
			case 'o':
				infoMode = !infoMode;
				break;
		}
		
		float frameTimeTarget = 33.3333f;
		frameTimeTarget = 50.00f;
		
		tp2 = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsedTime = tp2 - tp1;
		float elapsedTimef = std::chrono::duration_cast< std::chrono::microseconds>(elapsedTime).count() / 1000.0f;
		//std::chrono::duration<float, std::milli> elapsedTimef = t2 - t1;
		tp1 = tp2;
		
		
		
		playerYvelocity = playerYvelocity + (gravity * (1.0f/elapsedTimef));
		
		grounded = true;
		//collision check
		tileComplete below1 = world->getComplete(-playerX + 0.4f, -playerY+1);
		tileComplete below2 = world->getComplete(-playerX - 0.4f, -playerY+1);
		tileComplete physicsFrame1 = world->getComplete(-playerX + 0.4f, -(playerY + (playerYvelocity * (1.0f / elapsedTimef))) + 1);
		tileComplete physicsFrame2 = world->getComplete(-playerX - 0.4f, -(playerY + (playerYvelocity * (1.0f / elapsedTimef))) + 1);
		if (below1.parent->id == 0 && below2.parent->id == 0) {
			grounded = false;
		}
		if (physicsFrame1.parent->id != 0 && physicsFrame2.parent->id != 0 && !grounded)
			playerYvelocity = -1.0f;
		
		//physics
		//grounded = true;
		if (playerYvelocity < gravity)
			playerYvelocity = gravity;
		if (grounded == true && playerYvelocity < 0) {
			playerYvelocity = 0;
		}
		playerY += playerYvelocity * (1.0f / elapsedTimef);
		//if (playerY < -31)
		//	playerY = -31;
		
		
		viewBoxWidth = double(adv::width) / (2.0d * scale);
		viewBoxHeight = double(adv::height) / (1.0d * scale);
		
		viewX = playerX + (viewBoxWidth * 0.5f);
		viewY = playerY + (viewBoxHeight * 0.5f);
				
		display();
		if (elapsedTimef < frameTimeTarget)
			console::sleep(frameTimeTarget - elapsedTimef);
		//console::sleep(20);
		
		{
			char buf[50];
			snprintf(&buf[0], 49, "%.1f fps - %.1f ms ft", (1.0f/elapsedTimef)*1000.0f, elapsedTimef);
			adv::write(adv::width/2.0f-(strlen(&buf[0])/2.0f), 0, &buf[0]);
		}
		
		adv::draw();
	}
	
	//adv::construct.~constructor();
	
	return 0;
}