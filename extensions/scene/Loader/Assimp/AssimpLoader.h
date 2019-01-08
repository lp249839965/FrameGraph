// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#ifdef FG_ENABLE_ASSIMP

#include "scene/Loader/Intermediate/IntermediateScene.h"

namespace Assimp {
	class Importer;
}

namespace FG
{

	//
	// Assimp Scene Loader
	//

	class AssimpLoader final
	{
	// types
	public:
		struct Config
		{
			uint	maxTrianglesPerMesh	= UMax;
			uint	maxVerticesPerMesh	= UMax;

			bool	calculateTBN		= false;
			bool	smoothNormals		= false;
			bool	splitLargeMeshes	= false;
			bool	optimize			= false;
		};

		struct Statistic
		{
			/*size_t	staticNodesBatches { 0 };
			size_t	staticNodesVertices { 0 };
			size_t	staticNodesIndices { 0 };

			size_t	staticNodesMinBatchSize { size_t(-1) };	// indices
			size_t	staticNodesMaxBatchSize { 0 };			// indices
			size_t	staticNodesAvrBatchSize { 0 };			// indices

			size_t	uniqueMaterials { 0 };
			size_t	uniqueTextures { 0 };*/
		};

		using AssimpImporter_t = UniquePtr< Assimp::Importer >;


	// variables
	private:
		AssimpImporter_t			_importerPtr;


	// methods
	public:
		AssimpLoader ();
		~AssimpLoader ();

		ND_ IntermediateScenePtr  Load (const Config &config, StringView filename);
	};


}	// FG

#endif	// FG_ENABLE_ASSIMP
