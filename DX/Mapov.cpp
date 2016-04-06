// Mapov.cpp
// Lab 1 example, simple coloured triangle mesh
#include "Mapov.h"

Mapov::Mapov(HINSTANCE hinstance, HWND hwnd, int screenWidth, int screenHeight, Input *in) : BaseApplication(hinstance, hwnd, screenWidth, screenHeight, in)
{
	effectFlags = EFFECT_NONE;

	this->screenWidth = screenWidth;
	this->screenHeight = screenHeight;

	justGenerated = false;
	justCorrected = false;

	// Create Mesh object
	string filename = "../res/pallet_town.png";
	tileSize = 16;
	outputWidth = 512;
	outputHeight = 512;
	markovRadius = 3;
	corrections = 100;

	tilemap = CImg<int>(filename.c_str());
	window = CImgDisplay(CImg<unsigned char>(tilemap), "Mapov");

	tilesX = tilemap.width() / tileSize;
	tilesY = tilemap.height() / tileSize;

	if (outputWidth == -1)
	{
		outputWidth = tilemap.width();
	}
	if (outputHeight == -1)
	{
		outputHeight = tilemap.height();
	}

	generatedTilesX = outputWidth / tileSize;
	generatedTilesY = outputHeight / tileSize;

	GenerateTileData();

	// Analyse data in all directions for the markov radius
	for (int x = -markovRadius; x <= markovRadius; x++)
	{
		for (int y = -markovRadius; y <= markovRadius; y++)
		{
			if (x == 0 && y == 0) continue;

			tileChance.push_back(vector<vector<int>>());
			AnalyseTileData(x, y);
		}
	}

	tilemap.normalize(0, 255);
	tilemap.save("../res/GeneratedTilemap.png");

	ResizeMesh();

	m_RenderTexture = new RenderTexture(m_Direct3D->GetDevice(), screenWidth, screenHeight, 0.0f, 1000.0f);
	m_ScreenMesh = new OrthoMesh(m_Direct3D->GetDevice(), screenWidth, screenHeight, 0, 0);
	m_Texture = new Texture(m_Direct3D->GetDevice(), L"../res/GeneratedTilemap.png");
	m_HBlurShader = new HBlurShader(m_Direct3D->GetDevice(), hwnd);
	m_ProcessShader = new ProcessShader(m_Direct3D->GetDevice(), hwnd);
}

Mapov::~Mapov()
{
	// Run base application deconstructor
	BaseApplication::~BaseApplication();

	// Release the Direct3D object.
	if (m_Mesh)
	{
		delete m_Mesh;
		m_Mesh = 0;
	}

	if (m_HBlurShader)
	{
		delete m_HBlurShader;
		m_HBlurShader = 0;
	}

	if (m_ProcessShader)
	{
		delete m_ProcessShader;
		m_ProcessShader = 0;
	}
}


bool Mapov::Frame()
{
	bool result;

	result = BaseApplication::Frame();
	if (!result)
	{
		return false;
	}

	// Input
	if (m_Input->isKeyDown(VK_SPACE) && !justGenerated)
	{
		cout << "Begin generating" << endl;

		GenerateTileMap();

		DrawTileData();

		justGenerated = true;
	}
	else
	{
		if (justGenerated)
		{
			justGenerated = false;
		}
	}
	
	if (m_Input->isKeyDown('C') && !justCorrected)
	{
		CorrectTileMap();

		DrawTileData();

		justCorrected = true;
	}
	else 
	{
		if (justCorrected)
		{
			justCorrected = false;
		}
	}

	// Set effects
	if (m_Input->isKeyDown('1'))
	{
		effectFlags |= EFFECT_BLUR;
	}
	if (m_Input->isKeyDown('2'))
	{
		effectFlags |= EFFECT_EDGEDETECT;
	}
	if (m_Input->isKeyDown('3'))
	{
		effectFlags |= EFFECT_POSTERISE;
	}
	// Clear effects
	if (m_Input->isKeyDown('0'))
	{
		effectFlags = EFFECT_NONE;
	}

	// Render the graphics.
	result = Render();
	if (!result)
	{
		return false;
	}

	return true;
}

bool Mapov::Render()
{
	XMMATRIX worldMatrix, baseViewMatrix, orthoMatrix;
	
	// Clear the scene. (default blue colour)
	m_Direct3D->BeginScene(0.0f, 0.0f, 0.0f, 1.0f);

	// Generate the view matrix based on the camera's position.
	m_Camera->Update();

	// Get the world, view, projection, and ortho matrices from the camera and Direct3D objects.
	m_Direct3D->GetWorldMatrix(worldMatrix);
	m_Camera->GetBaseViewMatrix(baseViewMatrix);
	m_Direct3D->GetOrthoMatrix(orthoMatrix);

	if (false && (effectFlags & EFFECT_EDGEDETECT) == EFFECT_EDGEDETECT ||
		(effectFlags & EFFECT_BLUR) == EFFECT_BLUR)
	{
		// Horizontal Blur on render texture
		m_RenderTexture->SetRenderTarget(m_Direct3D->GetDeviceContext());

		// Clear the render to texture.
		m_RenderTexture->ClearRenderTarget(m_Direct3D->GetDeviceContext(), 0.0f, 0.0f, 0.0f, 1.0f);

		// Send geometry data (from mesh)
		m_ScreenMesh->SendData(m_Direct3D->GetDeviceContext());

		// Render horizontally blurred image to texture
		m_HBlurShader->SetShaderParameters(m_Direct3D->GetDeviceContext(), worldMatrix, baseViewMatrix, orthoMatrix, m_Texture->GetTexture(), tilemap.width(), tilemap.height());
		m_HBlurShader->Render(m_Direct3D->GetDeviceContext(), m_ScreenMesh->GetIndexCount());

		// Reset rendering to window
		m_Direct3D->SetBackBufferRenderTarget();

		// Send mesh data again
		m_Mesh->SendData(m_Direct3D->GetDeviceContext());

		// Set shader parameters (matrices and texture) using render texture
		m_ProcessShader->SetShaderParameters(m_Direct3D->GetDeviceContext(), worldMatrix, baseViewMatrix, orthoMatrix, m_RenderTexture->GetShaderResourceView(), m_RenderTexture->GetTextureWidth(), m_RenderTexture->GetTextureHeight(), effectFlags);

		// Render object (combination of mesh geometry and shader process
		m_ProcessShader->Render(m_Direct3D->GetDeviceContext(), m_Mesh->GetIndexCount());
	}
	else
	{
		// Send mesh data again
		m_Mesh->SendData(m_Direct3D->GetDeviceContext());

		// Set shader parameters (matrices and texture) using render texture
		m_ProcessShader->SetShaderParameters(m_Direct3D->GetDeviceContext(), worldMatrix, baseViewMatrix, orthoMatrix, m_Texture->GetTexture(), tilemap.width(), tilemap.height(), effectFlags);

		// Render object (combination of mesh geometry and shader process
		m_ProcessShader->Render(m_Direct3D->GetDeviceContext(), m_Mesh->GetIndexCount());
	}
	
	// Present the rendered scene to the screen.
	m_Direct3D->EndScene();

	return true;
}

void Mapov::ResizeMesh()
{
	float meshWidth, meshHeight;
	float tilemapRatio = float(tilemap.width()) / tilemap.height();
	float screenRatio = float(screenWidth) / screenHeight;
	
	if (tilemapRatio > screenRatio)
	{
		meshWidth = screenWidth;
		meshHeight = screenWidth / tilemapRatio;
	}
	else
	{
		meshWidth = screenHeight * tilemapRatio;
		meshHeight = screenHeight;
	}

	if (m_Mesh != nullptr)
	{
		delete m_Mesh;
	}

	m_Mesh = new OrthoMesh(m_Direct3D->GetDevice(), meshWidth, meshHeight, 0.0f, 0.0f);
}

void Mapov::DrawTileData()
{
	tilemap.resize(outputWidth, outputHeight);

	for (int tile = 0; tile < generatedTilemap.size(); tile++)
	{
		DrawTile(tile, generatedTilemap.at(tile));
	}

	tilemap.normalize(0, 255);
	tilemap.save("../res/GeneratedTilemap.png");

	ResizeMesh();

	delete m_Texture;
	m_Texture = new Texture(m_Direct3D->GetDevice(), L"../res/GeneratedTilemap.png");
}

void Mapov::DrawTile(int tilePosition, int tileSprite)
{
	int drawPixelX = (tilePosition % generatedTilesX) * tileSize;
	int drawPixelY = (tilePosition / generatedTilesX) * tileSize;

	tilemap.draw_image(drawPixelX, drawPixelY, 0, tileSprites.at(tileSprite));
}

void Mapov::CorrectTileMap()
{
	int tile = rand() % generatedTilemap.size();
	vector<int> incorrectTiles;
	int correctionCount = 0;

	for (int correction = 0; correction < corrections; correction++)
	{
		int currentTile = generatedTilemap.at(tile);

		for (int x = -markovRadius; x <= markovRadius; x++)
		{
			for (int y = -markovRadius; y <= markovRadius; y++)
			{
				if (x == 0 && y == 0) continue;

				int nextTile = IncorrectTile(x, y, tile, currentTile);

				if (nextTile != -1)
				{
					incorrectTiles.push_back(nextTile);
					correctionCount++;
				}
			}
		}

		if (incorrectTiles.size() == 0)
		{
			tile = rand() % generatedTilemap.size();
		}
		else
		{
			int randomIncorrectTile = rand() % incorrectTiles.size();
			tile = incorrectTiles.at(randomIncorrectTile);
			incorrectTiles.erase(incorrectTiles.begin() + randomIncorrectTile);
		}
	}

	cout << "corrections made: " << correctionCount << "\n";
}

int Mapov::IncorrectTile(int x, int y, int tile, int currentTile)
{
	int storedTile = Constrain(tile + x + generatedTilesX * y, 0, generatedTilesX * generatedTilesY - 1);

	if (GetChance(x, y, currentTile, generatedTilemap.at(storedTile)) == 0)
	{
		generatedTilemap[storedTile] = GenerateNextTile(currentTile, x, y);

		return storedTile;
	}

	return -1;
}

int Mapov::GetChance(int x, int y, int currentTile, int otherTile)
{
	int chanceList = GetChanceList(x, y);

	return tileChance.at(chanceList).at(currentTile).at(otherTile);
}

int Mapov::GetChanceList(int x, int y)
{
	int chanceList = 0;

	for (int _x = -markovRadius; _x <= markovRadius; _x++)
	{
		for (int _y = -markovRadius; _y <= markovRadius; _y++)
		{
			if (_x == 0 && _y == 0)
			{
				continue;
			}

			if (x == _x && y == _y)
			{
				return chanceList;
			}

			chanceList++;
		}
	}

	return chanceList;
}

void Mapov::GenerateTileMap()
{
	generatedTilemap.clear();
	vector<int> availableTiles;

	for (int tile = 0; tile < generatedTilesX * generatedTilesY; tile++)
	{
		generatedTilemap.push_back(-1);
		availableTiles.push_back(tile);
	}

	int currentTile = rand() % storedTiles.size();
	generatedTilemap[0] = currentTile;

	int tile = 0;

	while (HasValue(generatedTilemap, -1))
	{
		currentTile = generatedTilemap.at(tile);

		if (currentTile == -1)
		{
			currentTile = rand() % storedTiles.size();
			generatedTilemap[tile] = currentTile;
		}

		for (int x = -markovRadius; x <= markovRadius; x++)
		{
			for (int y = -markovRadius; y <= markovRadius; y++)
			{
				if (x == 0 && y == 0) continue;

				GenerateTile(x, y, tile, currentTile);
			}
		}

		tile = rand() % (generatedTilesX * generatedTilesY);
	}
}

void Mapov::GenerateTile(int x, int y, int tile, int currentTile)
{
	if (generatedTilemap.at(Constrain(tile + x + generatedTilesX * y, 0, generatedTilesX * generatedTilesY - 1)) == -1)
	{
		generatedTilemap[Constrain(tile + x + generatedTilesX * y, 0, generatedTilesX * generatedTilesY - 1)] = GenerateNextTile(currentTile, x, y);
	}
}

int Mapov::GenerateNextTile(int currentTile, int x, int y)
{
	int chanceList = GetChanceList(x, y);

	vector<int> tileChances;

	for (int otherTileIndex = 0; otherTileIndex < tileChance.at(chanceList).at(currentTile).size(); otherTileIndex++)
	{
		for (int chance = 0; chance < tileChance.at(chanceList).at(currentTile).at(otherTileIndex); chance++)
		{
			tileChances.push_back(otherTileIndex);
		}
	}

	int randomIndex = rand() % tileChances.size();

	return tileChances.at(randomIndex);
}

void Mapov::AnalyseTileData(int x, int y)
{
	int chanceList = GetChanceList(x, y);

	for (int storedTile = 0; storedTile < storedTiles.size(); storedTile++)
	{
		tileChance.at(chanceList).push_back(vector<int>());

		for (int storedTileChance = 0; storedTileChance < storedTiles.size(); storedTileChance++)
		{
			tileChance.at(chanceList).at(tileChance.at(chanceList).size() - 1).push_back(0);
		}
	}

	for (int tile = 0; tile < tilemapData.size(); tile++)
	{
		int currentTileIndex = tilemapData.at(tile);
		int nextTileIndex = tilemapData.at(Constrain((tile + x + (y * tilesX)), 0, tilemapData.size() - 1));
		int currentTileChance = tileChance.at(chanceList).at(currentTileIndex).at(nextTileIndex);
		tileChance.at(chanceList).at(currentTileIndex)[nextTileIndex] = currentTileChance + 1;
	}
}

void Mapov::GenerateTileData()
{
	int duplicateTiles = 0;

	for (int tileY = 0; tileY < tilesY; tileY++)
	{
		for (int tileX = 0; tileX < tilesX; tileX++)
		{
			int tileID = -1;
			int pixelSum = SumPixels(tileX, tileY);

			for (int storedTile = 0; storedTile < storedTiles.size(); storedTile++)
			{
				if (storedPixelSums.at(storedTile) == pixelSum)
				{
					tileID = storedTile;

					duplicateTiles++;
				}
			}

			if (tileID == -1)
			{
				storedTiles.push_back(tileY * tilesX + tileX);
				storedPixelSums.push_back(pixelSum);
				CImg<int> sprite = tilemap;
				tileSprites.push_back(sprite.crop(tileX * tileSize, tileY * tileSize, tileX * tileSize + tileSize, tileY * tileSize + tileSize));
				tileID = storedTiles.size() - 1;
			}

			tilemapData.push_back(tileID);
		}
	}

	cout << "duplicate tiles: " << duplicateTiles << "\n";
	cout << "stored tiles: " << storedTiles.size() << "\n";
}

int Mapov::SumPixels(int tileX, int tileY)
{
	int sum = 0;
	CImg<int> tilePixels = tilemap;
	tilePixels.crop(tileX * tileSize, tileY * tileSize, tileX * tileSize + tileSize, tileY * tileSize + tileSize);

	for (int pixelY = 0; pixelY < tileSize; pixelY++)
	{
		for (int pixelX = 0; pixelX < tileSize; pixelX++)
		{
			sum += (tilePixels(pixelX, pixelY, 0, RED) + tilePixels(pixelX, pixelY, 0, GREEN) + tilePixels(pixelX, pixelY, 0, BLUE)) * pixelX * pixelY;
		}
	}

	return sum;
}

int Mapov::Constrain(int value, int lower, int upper)
{
	if (value < lower)
	{
		return lower;
	}
	else if (value > upper)
	{
		return upper;
	}

	return value;
}

bool Mapov::HasValue(vector<int> list, int value)
{
	for each (int item in list)
	{
		if (item == value)
		{
			return true;
		}
	}

	return false;
}