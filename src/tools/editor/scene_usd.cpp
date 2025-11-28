#ifdef USD_SUPPORT
#include "tinyusdz.hh"
#include "pprinter.hh"
#include "value-pprint.hh"
#include <iostream>
#include <string>
#include "scene.h"

void LoadUsdScene( const char* path )
{
    tinyusdz::Stage stage;
    std::string warn, err;
    
    bool ret = tinyusdz::LoadUSDFromFile( path, &stage, &warn, &err );

    if (warn.size())
    {
        std::cout << "WARN : " << warn << "\n";
    }

    if (!ret)
    {
        if (!err.empty())
        {
            std::cerr << "ERR : " << warn << "\n";
        }
        return;
    }

    // Print Stage(Scene graph)
    std::cout << tinyusdz::to_string(stage) << "\n";
  
    // You can also use ExportToString() as done in pxrUSD 
    // std::cout << stage.ExportToString() << "\n";

    // stage.metas() To get Scene metadatum, 
    for (const tinyusdz::Prim &root_prim : stage.root_prims())
    {
        std::cout << root_prim.absolute_path() << "\n";
        // You can traverse Prim(scene graph object) using Prim::children()
        // See examples/api_tutorial and examples/tydra_api for details.
    }

    return;
}
#endif
