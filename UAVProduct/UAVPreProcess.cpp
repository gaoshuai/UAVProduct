//
// Created by wuwei on 17-11-6.
//

#include <omp.h>
#include <memory>

#include "cereal/archives/json.hpp"

#include "UAVPreProcess.h"
#include "openMVG/system/timer.hpp"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "openMVG/matching_image_collection/Pair_Builder.hpp"

#include "openMVG/matching/matcher_brute_force.hpp"
#include "openMVG/exif/exif_IO_EasyExif.hpp"

#include "openMVG/stl/stl.hpp"
#include "openMVG/features/regions_factory.hpp"
#include "nonFree/sift/SIFT_describer.hpp"
#include "nonFree/sift/SIFT_describer_io.hpp"
#include "openMVG/features/sift/SIFT_Anatomy_Image_Describer.hpp"
#include "openMVG/matching_image_collection/Matcher_Regions.hpp"
#include "openMVG/matching_image_collection/Cascade_Hashing_Matcher_Regions.hpp"
#include "openMVG/matching/pairwiseAdjacencyDisplay.hpp"
#include "openMVG/matching_image_collection/E_ACRobust.hpp"

#include "openMVG/image/image_io.hpp"

#include "openMVG/graph/graph.hpp"

#include "openMVG/matching_image_collection/GeometricFilter.hpp"



#ifdef _DEBUG
#define LOG( format, args... )  printf( format, ##args )
#else
#define LOG( format, args... ) NULL;
#endif


UAVErr UAVProcessPOS::UAVProcessPOSExtractXYZ(double &centerx, double &centery, double &centerz) {
    size_t size = posList.size();
    if(size==0)
        return 1;

    POSPair::iterator iter;
    centerx=centery=centerz=0;
    for( iter=posList.begin(); iter!=posList.end(); iter++)
    {
        openMVG::Vec3 xyz=UAVProcessGeometry::UAVProcessGeoBLHToXYZ(iter->second.dB, iter->second.dL, iter->second.dH);
        iter->second.dB=xyz(0);
        iter->second.dL=xyz(1);
        iter->second.dH=xyz(2);
        centerx += xyz(0);
        centery += xyz(1);
        centerz += xyz(2);
    }
    centerx/=double(size);
    centery/=double(size);
    centerz/=double(size);

    return 0;
}

UAVErr UAVProcessPOS::UAVProcessPOSExtractUTM(double &centerx, double &centery, double &centerz) {
    size_t size = posList.size();
    if(size==0)
        return 1;

    POSPair::iterator iter;
    centerx=centery=centerz=0;
    for( iter=posList.begin(); iter!=posList.end(); iter++)
    {
        openMVG::Vec3 utm=UAVProcessGeometry::UAVProcessGeoLatLonToUTM(iter->second.dB, iter->second.dL, iter->second.dH);
        iter->second.dB=utm(0);
        iter->second.dL=utm(1);
        iter->second.dH=utm(2);
        centerx += utm(0);
        centery += utm(1);
        centerz += utm(2);
    }
    centerx/=double(size);
    centery/=double(size);
    centerz/=double(size);



    return 0;
}

UAVErr UAVProcessPOS::UAVProcessPOSExtractLocal(double &centerx, double &centery, double &centerz) {
    size_t size = posList.size();
    if(size==0)
        return 1;

    POSPair::iterator iter;
    centerx=centery=centerz=0;
    for( iter=posList.begin(); iter!=posList.end(); iter++)
    {
        openMVG::Vec3 utm=UAVProcessGeometry::UAVProcessGeoLatLonToUTM(iter->second.dB, iter->second.dL, iter->second.dH);
        iter->second.dB=utm(0);
        iter->second.dL=utm(1);
        iter->second.dH=utm(2);
        centerx += utm(0);
        centery += utm(1);
        centerz += utm(2);
    }
    centerx/=double(size);
    centery/=double(size);
    centerz/=double(size);

    for( iter=posList.begin(); iter!=posList.end(); iter++)
    {
        //openMVG::Vec3 utm=UAVProcessGeometry::UAVProcessGeoLatLonToUTM(iter->second.dB, iter->second.dL, iter->second.dH);
        iter->second.dB-=centerx;
        iter->second.dL-=centery;
        //iter->second.dH-=centerz;
    }

    return 0;
}

UAVErr UAVProcessPOSSimple::UAVPorcessPOSGet(std::string file, bool bGps) 
{
    UAVErr err = 0;
    //整理数据获取影像数和POS数据行数
    if(!bGps)
    {
        std::string image_dir = file;
        if ( !stlplus::folder_exists( image_dir) )
        {
            return 1;
        }
        std::vector<std::string> vec_image = stlplus::folder_files(image_dir);
        std::sort(vec_image.begin(), vec_image.end());
        std::unique_ptr<openMVG::exif::Exif_IO> exifReader(new openMVG::exif::Exif_IO_EasyExif);
        for(int i=0;i<vec_image.size();++i)
        {
            if( exifReader->open( stlplus::create_filespec(image_dir,vec_image[i]) ) && exifReader->doesHaveExifInfo())
            {
                double latitude, longitude, altitude;
                if ( exifReader->GPSLatitude( &latitude ) &&
                     exifReader->GPSLongitude( &longitude ) &&
                     exifReader->GPSAltitude( &altitude ) )
                {
                    UAVPOSSt tmpPos;
                    tmpPos.dB=latitude;tmpPos.dL=longitude;tmpPos.dH=altitude;
                    tmpPos.dRoll=tmpPos.dPitch=tmpPos.dHeading=0;
                    posList.insert(std::make_pair(i+1,tmpPos));
                } 
                else
                {
                    return 1;
                }
            }
            else
                return 1;
        }
        return 0;
    } 
    else
    {
        if(!stlplus::file_exists(file))
            return 1;
        else
        {
            FILE* ptrfile=NULL;
            ptrfile = fopen(file.c_str(),"r+");
            if(ptrfile==NULL)
                return 1;
            char posline[2048];
            int num=1;
            while(!feof(ptrfile))
            {
                UAVPOSSt tmpPos;
                fgets(posline,2048,ptrfile);
                sscanf(posline,"%lf%lf%lf%lf%lf%lf",&tmpPos.dL,&tmpPos.dB,&tmpPos.dH,&tmpPos.dRoll,&tmpPos.dPitch,&tmpPos.dHeading);
                posList.insert(std::make_pair(num,tmpPos));
                num++;
            };
            fclose(ptrfile);
            ptrfile=NULL;
            return 0;
        }
    }
}


//TODO:
//POS数据导出的功能暂时先不做
UAVErr UAVProcessPOSSimple::UAVProcessExport(std::string file, bool rLoc) 
{
    return 0;
}

UAVErr UAVProcessList::UAVProcessListGet(std::string image_dir,std::string sfm_out,
                                         UAVCalibParams &cParam,EINTRINSIC camera_model_type,bool group_camera_model,
                                         std::string pos_file, UAVProcessPOS *pPorc, CoordiListType typeCoordi) 
{
    //检查数据
    //特征点解算列表
    if(!stlplus::folder_exists(image_dir))
    {
        std::cerr<<"The input directory doesn't exist"<<std::endl;
        return 1;
    }

    //check sfm_out path
    if(stlplus::extension_part(sfm_out)!="json")//although the sfm_data support others format, I only add ".json" to make the program simple
    {
        std::cerr<<"invalid file format"<<std::endl;
        return 1;
    }
    if(!stlplus::folder_exists(stlplus::folder_part(sfm_out)))//check whether the sfm_out folder exist, if not create it
    {
        if(!stlplus::folder_create(stlplus::folder_part(sfm_out)))
        {
            std::cerr<<"cannot create output directory"<<std::endl;
            return 1;
        }
    }

    std::vector<std::string> vec_image = stlplus::folder_files(image_dir);
    std::sort(vec_image.begin(), vec_image.end());

    using namespace openMVG::sfm;

    //sfm data
    SfM_Data sfm_data;
    sfm_data.s_root_path =  image_dir;
    Views & views = sfm_data.views;
    Intrinsics & intrinsics = sfm_data.intrinsics;


    //获取POS数据
    bool have_no_pose=true;
    if(stlplus::file_exists(pos_file))
    {
        //get pos from file
        have_no_pose=pPorc->UAVPorcessPOSGet(pos_file,true);
    } 
    else
    {
        //get pos from image
        have_no_pose=pPorc->UAVPorcessPOSGet(image_dir,false);
    }
    double center_X,center_Y,center_Z;
    if(!have_no_pose)
    {
        switch (typeCoordi)
        {
            case CoordinateXYZ:
                pPorc->UAVProcessPOSExtractXYZ(center_X,center_Y,center_Z);
                break;
            case CoordinateUTM:
                pPorc->UAVProcessPOSExtractUTM(center_X,center_Y,center_Z);
                break;
            case CoordinateLocal:
                pPorc->UAVProcessPOSExtractLocal(center_X,center_Y,center_Z);
                break;
            default:
                have_no_pose=true;
        }
    }
  
    //check pos's number
    //if(!have_no_pose && pPorc->posList.size()!=vec_image.size())
        //return 1;

    bool bCalibParam=IsUndefineCalibParam(cParam);
    double width,height,ppx,ppy,focal;

    POSPair::iterator pos_iter = pPorc->posList.begin();
    for (std::vector<std::string>::const_iterator iter_image = vec_image.begin();
        iter_image != vec_image.end();
        ++iter_image)
    {
        ppx = ppy = width = height = focal = -1;

        const std::string sImageFilename = stlplus::create_filespec(image_dir, *iter_image );
        const std::string sImFilenamePart = stlplus::filename_part(sImageFilename);

        openMVG::image::ImageHeader imgHeader;
        if (!openMVG::image::ReadImageHeader(sImageFilename.c_str(), &imgHeader))
            continue;

        width = imgHeader.width;
        height = imgHeader.height;
        ppx = width / 2.0;
        ppy = height / 2.0;
        if(bCalibParam)
        {
            ppx=cParam._ppx_;
            ppy=cParam._ppy_;
            focal=std::max(cParam._flen_x_,cParam._flen_y_);
        }
        else
        {
            ppx=width/2.0;
            ppy=height/2.0;
            std::unique_ptr<openMVG::exif::Exif_IO> exifPosReader(new openMVG::exif::Exif_IO_EasyExif);
            if(exifPosReader->open( sImageFilename ))
                focal = std::max ( width, height ) * exifPosReader->getFocal() / exifPosReader->getFocal();
            else
                focal=std::max(width,height);
        }

        std::unique_ptr<openMVG::exif::Exif_IO> exifReader(new openMVG::exif::Exif_IO_EasyExif);
        exifReader->open( sImageFilename );
        focal = std::max ( width, height );

        std::shared_ptr<openMVG::cameras::IntrinsicBase> intrinsic(NULL);
        if( focal>0 && ppx>0 && ppy>0 && height>0 && width>0 )
        {
            //initial intrinsic
            //get from openMVG
            switch (camera_model_type)
            {
                case PINHOLE_CAMERA:
                intrinsic = std::make_shared<openMVG::cameras::Pinhole_Intrinsic>
                    (width, height, focal, ppx, ppy);
                break;
                case PINHOLE_CAMERA_RADIAL1:
                intrinsic = std::make_shared<openMVG::cameras::Pinhole_Intrinsic_Radial_K1>
                    (width, height, focal, ppx, ppy, 0.0); // setup no distortion as initial guess
                break;
                case PINHOLE_CAMERA_RADIAL3:
                intrinsic = std::make_shared<openMVG::cameras::Pinhole_Intrinsic_Radial_K3>
                    (width, height, focal, ppx, ppy, 0.0, 0.0, 0.0);  // setup no distortion as initial guess
                break;
                case PINHOLE_CAMERA_BROWN:
                intrinsic = std::make_shared<openMVG::cameras::Pinhole_Intrinsic_Brown_T2>
                    (width, height, focal, ppx, ppy, 0.0, 0.0, 0.0, 0.0, 0.0); // setup no distortion as initial guess
                break;
                default:
                std::cerr << "Error: unknown camera model: " << (int) camera_model_type << std::endl;
                return 1;
            }
        }

        if(have_no_pose)
        {
            View v(*iter_image, views.size(), views.size(), views.size(), width, height);
            if (intrinsic == NULL)
                v.id_intrinsic = openMVG::UndefinedIndexT;
            else
                intrinsics[v.id_intrinsic] = intrinsic;
            // Add the view to the sfm_container
            views[v.id_view] = std::make_shared<View>(v);
        }
        else
        {
            ViewPriors v(*iter_image, views.size(), views.size(), views.size(), width, height);
            // Add intrinsic related to the image (if any)
            if (intrinsic == NULL)
                v.id_intrinsic = openMVG::UndefinedIndexT;
            else
                intrinsics[v.id_intrinsic] = intrinsic;

            // Add the view to the sfm_container
            views[v.id_view] = std::make_shared<View>(v);
            v.b_use_pose_center_ = true;
            double x = pos_iter->second.dL;
            double y = pos_iter->second.dB;
            double z = pos_iter->second.dH;
            v.pose_center_ = openMVG::Vec3(x,y,z);
            views[v.id_view] = std::make_shared<ViewPriors>(v);
            ++pos_iter;
        }

    }

    //group camera that share common properties
    //make BS more stable and faster
    if(group_camera_model)
    {
        openMVG::sfm::GroupSharedIntrinsics(sfm_data);
    }

    if (!Save(
            sfm_data,
            sfm_out.c_str(),
            ESfM_Data(VIEWS|INTRINSICS)))
    {
        return 3;
    }

    return 0;

}
//matches
UAVErr UAVProcessMatches::UAVProcessMatchesList(std::string imageList, int neighbor_count, bool bGeo,
                                                std::string pMatch) {
    std::string imgLst=imageList;
    std::string matchList=pMatch;
    int i_neighbor_count = neighbor_count;
    MATCHMODEL i_mode=PAIR_MODE_EXHAUSTIVE;

    //首先check输入
    if(!stlplus::file_exists(imgLst)||pMatch.empty())
        return 1;
    //if(bGeo&&neighbor_count==0)
    //    return 1;

    if (i_neighbor_count==0)
        i_mode = PAIR_MODE_EXHAUSTIVE;
    else if (i_neighbor_count>=1&&!bGeo)
        i_mode = PAIR_MODE_CONTIGUOUS;
    else if (i_neighbor_count>=1&&bGeo)
        i_mode = PAIR_MODE_NEIGHBORHOOD;

    openMVG::sfm::SfM_Data sfm_data;
    if (!Load(sfm_data, imgLst, openMVG::sfm::ESfM_Data(openMVG::sfm::VIEWS|openMVG::sfm::INTRINSICS)))
    {
        std::cerr << std::endl
                  << "The input SfM_Data file \"" << imgLst << "\" cannot be read." << std::endl;
        return 1;
    }
    LOG("Loaded a sfm_data scene with:\n#views: %d\n",sfm_data.GetViews().size());
    // out file


    //---------------------------------------
    // a. List the view pose as a linear sequence of ids.
    // b. Establish a pose graph according the user chosen mode:
    //    - E => upper diagonal pairs,
    //    - V => list the N closest pose ids,
    //    - G => list the N closest poses XYZ position.
    // c. Convert the pose graph edges to a view graph
    // d. Export the view graph to a file and a SVG adjacency list
    //---------------------------------------
    std::multimap<openMVG::IndexT, openMVG::IndexT> pose_id_toViewId;
    std::set<openMVG::IndexT> set_poses;

    // a. Get nodes of the pose graph as a linear sequence
    for (const auto & viewIter : sfm_data.GetViews())
    {
        const openMVG::sfm::View * v = viewIter.second.get();
        assert (viewIter.first == v->id_view);
        pose_id_toViewId.insert( std::make_pair(v->id_pose, v->id_view) );
        set_poses.insert(v->id_pose);
    }
    const std::vector<openMVG::IndexT> vec_poses(set_poses.begin(), set_poses.end());


    // b. Create the pose graph pair relationship
    MatchesList pose_pairs;

    switch (i_mode)
    {
        case PAIR_MODE_EXHAUSTIVE:
            //所有节点全部连接 连接矩阵的上半部分全为1
            pose_pairs = openMVG::exhaustivePairs(sfm_data.GetViews().size());
            break;
        case PAIR_MODE_CONTIGUOUS:
            //连续的几张影像有重叠
            pose_pairs = openMVG::contiguousWithOverlap(vec_poses.size(), i_neighbor_count);
            break;
        case PAIR_MODE_NEIGHBORHOOD:
        {
            // List the poses priors 根据POS计算相邻n个邻域范围内的影像
            std::vector<openMVG::Vec3> vec_pose_centers;
            std::map<openMVG::IndexT, openMVG::IndexT> contiguous_to_pose_id;
            std::set<openMVG::IndexT> used_pose_ids;
            for (const auto & view_it : sfm_data.GetViews())
            {
                const openMVG::sfm::ViewPriors * prior = dynamic_cast<openMVG::sfm::ViewPriors*>(view_it.second.get());
                if (prior != nullptr && prior->b_use_pose_center_ && used_pose_ids.count(prior->id_pose) == 0)
                {
                    vec_pose_centers.push_back( prior->pose_center_ );
                    contiguous_to_pose_id[contiguous_to_pose_id.size()] = prior->id_pose;
                    used_pose_ids.insert(prior->id_pose);
                }
            }
            if (vec_pose_centers.empty())
            {
                std::cerr << "You are trying to use the gps_mode but your data does"
                          << " not have any pose priors."
                          << std::endl;
                return 1;
            }
            // Compute i_neighbor_count neighbor(s) for each pose
            size_t contiguous_pose_id = 0;
            for (const openMVG::Vec3 pose_it : vec_pose_centers)
            {
                openMVG::matching::ArrayMatcherBruteForce<double> matcher;
                if (matcher.Build(vec_pose_centers[0].data(), vec_pose_centers.size(), 3))
                {
                    const double * query = pose_it.data();

                    openMVG::matching::IndMatches vec_indices;
                    std::vector<double> vec_distance;
                    const int NN = i_neighbor_count + 1; // since itself will be found
                    if (matcher.SearchNeighbours(query, 1, &vec_indices, &vec_distance, NN))
                    {
                        for (size_t i = 1; i < vec_indices.size(); ++i)
                        {
                            openMVG::IndexT idxI = contiguous_to_pose_id.at(contiguous_pose_id);
                            openMVG::IndexT idxJ = contiguous_to_pose_id.at(vec_indices[i].j_);
                            if (idxI > idxJ)
                                std::swap(idxI, idxJ);
                            pose_pairs.insert(openMVG::Pair(idxI, idxJ));
                        }
                    }
                }
                ++contiguous_pose_id;
            }
        }
            break;
        default:
            //测试用例无法覆盖
            std::cerr << "Unknown pair mode." << std::endl;
            return 4;
    }


    // c. Convert the pose graph to a view graph
    MatchesList view_pair;
    for (const auto & pose_pair : pose_pairs)
    {
        const openMVG::IndexT poseA = pose_pair.first;
        const openMVG::IndexT poseB = pose_pair.second;
        // get back the view related to those poses and create the pair (exhaustively)
        const auto range_a = pose_id_toViewId.equal_range(vec_poses[poseA]);
        for (auto view_id_a = range_a.first; view_id_a != range_a.second; view_id_a++)
        {
            const auto range_b = pose_id_toViewId.equal_range(vec_poses[poseB]);
            for (auto view_id_b = range_b.first; view_id_b != range_b.second; view_id_b++)
            {
                if (view_id_a != view_id_b)
                {
                    view_pair.insert(
                            openMVG::Pair(std::min(view_id_a->second, view_id_b->second),
                                 std::max(view_id_a->second, view_id_b->second)));
                }
            }
        }
    }

    if (view_pair.empty())
    {
        LOG("Warning: The computed pair list is empty...!");
    }

    // d. Export the view graph to a file and a SVG adjacency list

    UAVProcessAdjacencyMatrixToSVG(sfm_data.GetViews().size(), view_pair,
                         stlplus::create_filespec(
                                 stlplus::folder_part(pMatch),
                                 stlplus::filename_part(pMatch), "svg"));

    if (openMVG::savePairs(pMatch, view_pair))
    {
        LOG("Exported %d view pairs\n from a view graph that have %d relative pose pairs.\n" ,view_pair.size(),pose_pairs.size());
        return 0;
    }

    return 4;

}
void UAVProcessMatches::UAVProcessAdjacencyMatrixToSVG(const size_t NbImages,
                const openMVG::Pair_Set & corresponding_indexes,
                const std::string & sOutName)
{
    using namespace svg;
    if (!corresponding_indexes.empty())
    {
        const float scaleFactor = 5.0f;
        svgDrawer svgStream((NbImages+3)*5, (NbImages+3)*5);
        // List possible pairs
        for (openMVG::IndexT I = 0; I < NbImages; ++I)
        {
            for (openMVG::IndexT J = 0; J < NbImages; ++J)
            {
                // If the pair have matches display a blue boxes at I,J position.
                const auto iterSearch = corresponding_indexes.find(std::make_pair(I,J));
                if (iterSearch != corresponding_indexes.end())
                {
                    svgStream.drawSquare(J*scaleFactor, I*scaleFactor, scaleFactor/2.0f,
                                         svgStyle().fill("blue").noStroke());
                }
            }
        }
        // Display axes with 0 -> NbImages annotation : _|
        std::ostringstream osNbImages;
        osNbImages << NbImages;
        svgStream.drawText((NbImages+1)*scaleFactor, scaleFactor, scaleFactor, "0", "black");
        svgStream.drawText((NbImages+1)*scaleFactor,
                           (NbImages)*scaleFactor - scaleFactor, scaleFactor, osNbImages.str(), "black");
        svgStream.drawLine((NbImages+1)*scaleFactor, 2*scaleFactor,
                           (NbImages+1)*scaleFactor, (NbImages)*scaleFactor - 2*scaleFactor,
                           svgStyle().stroke("black", 1.0));

        svgStream.drawText(scaleFactor, (NbImages+1)*scaleFactor, scaleFactor, "0", "black");
        svgStream.drawText((NbImages)*scaleFactor - scaleFactor,
                           (NbImages+1)*scaleFactor, scaleFactor, osNbImages.str(), "black");
        svgStream.drawLine(2*scaleFactor, (NbImages+1)*scaleFactor,
                           (NbImages)*scaleFactor - 2*scaleFactor, (NbImages+1)*scaleFactor,
                           svgStyle().stroke("black", 1.0));

        std::ofstream svgFileStream(sOutName.c_str());
        svgFileStream << svgStream.closeSvgFile().str();
    }
}

UAVErr UAVProcessMatches::UAVProcessMatchesExport(MatchesList list, std::string pMatch) {
    if (openMVG::savePairs(pMatch, list))
    {
        LOG("Exported %d matches failed\n");
        return 0;
    }
    return 4;
}

UAVErr UAVProcessMatches::UAVProcessMatchesImport(MatchesList &list, std::string pMatch) {
    return 0;
}

UAVErr UAVProcessFeature::UAVProcessFeatList(std::string sfm_data_path,std::string feature_dir) 
{
    //判断输入
    if(!stlplus::file_exists(sfm_data_path))
        return 1;
    if(!stlplus::folder_exists(feature_dir))
    {
        if(!stlplus::folder_create(feature_dir))
            return 1;
    }

    sfm_data_path_ = sfm_data_path;

    openMVG::sfm::SfM_Data sfm_data;
    if (!Load(sfm_data, sfm_data_path, openMVG::sfm::ESfM_Data(openMVG::sfm::VIEWS|openMVG::sfm::INTRINSICS)))
    {
        std::cerr << std::endl
                  << "The input SfM_Data file \"" << sfm_data_path << "\" cannot be read." << std::endl;
        return 1;
    }
    LOG("Loaded a sfm_data scene with:\n#views: %d\n",sfm_data.GetViews().size());

    int i=0;
    for (const auto & viewIter : sfm_data.GetViews())
    {
        const openMVG::sfm::View * view = viewIter.second.get();
        const std::string
                sView_filename = stlplus::create_filespec(sfm_data.s_root_path, view->s_Img_path),
                sFeat = stlplus::create_filespec(feature_dir, stlplus::basename_part(sView_filename), "feat"),
                sDesc = stlplus::create_filespec(feature_dir, stlplus::basename_part(sView_filename), "desc");
        FeatureParam featsParam;
        featsParam._image_in_ = sView_filename;
        featsParam._feature_out_= sFeat;
        featsParam._descs_out_= sDesc;
        this->feature[i++]=featsParam;
    }
    return 0;
}

UAVErr UAVProcessFeatureSIFT::UAVProcessMatchesExtract(std::string pMatchList,std::string pMatchData) 
{
    std::string pList = pMatchList;
    std::string pMatchDir = stlplus::folder_part(pMatchData);

    if(!stlplus::file_exists(pMatchList))
        return 1;

    //读取matchlist
    std::unique_ptr<openMVG::features::Image_describer> image_describer;
    image_describer.reset(new openMVG::features::SIFT_Image_describer
                                  (openMVG::features::SIFT_Image_describer::Params(), true));
    
    const std::string sImage_describer = stlplus::create_filespec(pMatchDir,"image_describer",".json");
    {
        std::ofstream stream(sImage_describer.c_str());
        if (!stream.is_open())
            return 6;
        cereal::JSONOutputArchive archive(stream);
        archive(cereal::make_nvp("image_describer", image_describer));
        auto regions_type_out= image_describer->Allocate();
        archive(cereal::make_nvp("regions_type", regions_type_out));
    }

    std::unique_ptr<openMVG::features::Regions> regions_type_in = openMVG::features::Init_region_type_from_file(sImage_describer);

    openMVG::sfm::SfM_Data sfm_data;
    if (!Load(sfm_data, sfm_data_path_, openMVG::sfm::ESfM_Data(openMVG::sfm::VIEWS|openMVG::sfm::INTRINSICS))) 
    {
        std::cerr << std::endl
                  << "The input SfM_Data file \""<< sfm_data_path_ << "\" cannot be read." << std::endl;
        return false;
    }


    std::shared_ptr<openMVG::sfm::Regions_Provider> regions_provider;
    regions_provider = std::make_shared<openMVG::sfm::Regions_Provider>();
    regions_provider->load(sfm_data,pMatchDir,regions_type_in);

    //进行匹配
    openMVG::matching::PairWiseMatches map_PutativesMatches;
    //判断是否存在
    if(stlplus::file_exists(pMatchData))
    {
        if(openMVG::matching::Load(map_PutativesMatches,pMatchData))
            return 0;
    }

    //不存在则需要重新匹配 SIFT对应的不需要进行判断了
    float fDistRatio=0.8f;
    std::unique_ptr<openMVG::matching_image_collection::Matcher> collectionMatcher;
    std::cout << "Using FAST_CASCADE_HASHING_L2 matcher" << std::endl;
    collectionMatcher.reset(new openMVG::matching_image_collection::Cascade_Hashing_Matcher_Regions(fDistRatio));

    openMVG::system::Timer timer;
    {
        // From matching mode compute the pair list that have to be matched:
        openMVG::Pair_Set pairs;
        if(!openMVG::loadPairs(feature.size(), pList, pairs))
        {
            return 5;
        }

        // Photometric matching of putative pairs
        collectionMatcher->Match(sfm_data, regions_provider, pairs, map_PutativesMatches);
        //---------------------------------------
        //-- Export putative matches
        //---------------------------------------
        if (!Save(map_PutativesMatches, std::string(pMatchData)))
        {
            std::cerr
                    << "Cannot save computed matches in: "
                    << pMatchData;
            return 5;
        }

        //-- export putative matches Adjacency matrix
        openMVG::matching::PairWiseMatchingToAdjacencyMatrixSVG(feature.size(),
                                             map_PutativesMatches,
                                             stlplus::create_filespec(stlplus::folder_part(pMatchData), "PutativeAdjacencyMatrix", "svg"));
        //-- export view pair graph once putative graph matches have been computed
        {
            std::set<openMVG::IndexT> set_ViewIds;
            std::transform(sfm_data.GetViews().begin(), sfm_data.GetViews().end(),
                           std::inserter(set_ViewIds, set_ViewIds.begin()), stl::RetrieveKey());
            openMVG::graph::indexedGraph putativeGraph(set_ViewIds, getPairs(map_PutativesMatches));
            openMVG::graph::exportToGraphvizData(
                    stlplus::create_filespec(stlplus::folder_part(pMatchData), "putative_matches"),
                    putativeGraph);
        }

        //---------------------------------------
        // b. Geometric filtering of putative matches
        //    - AContrario Estimation of the desired geometric model
        //    - Use an upper bound for the a contrario estimated threshold
        //---------------------------------------

        std::unique_ptr<openMVG::matching_image_collection::ImageCollectionGeometricFilter> filter_ptr(
                new openMVG::matching_image_collection::ImageCollectionGeometricFilter(&sfm_data, regions_provider));
        int imax_iteration = 2048;
        bool bGuided_matching = false;
        if (filter_ptr)
        {
            openMVG::matching::PairWiseMatches map_GeometricMatches;
            filter_ptr->Robust_model_estimation(openMVG::matching_image_collection::GeometricFilter_EMatrix_AC(4.0, imax_iteration),
                                                map_PutativesMatches, bGuided_matching);
            map_GeometricMatches = filter_ptr->Get_geometric_matches();

            //-- Perform an additional check to remove pairs with poor overlap
            std::vector<openMVG::matching::PairWiseMatches::key_type> vec_toRemove;
            for (openMVG::matching::PairWiseMatches::const_iterator iterMap = map_GeometricMatches.begin();
                 iterMap != map_GeometricMatches.end(); ++iterMap)
            {
                const size_t putativePhotometricCount = map_PutativesMatches.find(iterMap->first)->second.size();
                const size_t putativeGeometricCount = iterMap->second.size();
                const float ratio = putativeGeometricCount / (float)putativePhotometricCount;
                if (putativeGeometricCount < 50 || ratio < .3f)  {
                    // the pair will be removed
                    vec_toRemove.push_back(iterMap->first);
                }
            }
            //-- remove discarded pairs
            for (std::vector<openMVG::matching::PairWiseMatches::key_type>::const_iterator
                         iter =  vec_toRemove.begin(); iter != vec_toRemove.end(); ++iter)
            {
                map_GeometricMatches.erase(*iter);
            }
            std::string sGeometricMatchesFilename = "matches.h.txt";
            //---------------------------------------
            //-- Export geometric filtered matches
            //---------------------------------------
            if (!openMVG::matching::Save(map_GeometricMatches,
                      std::string(stlplus::folder_part(pMatchData) + "/" + sGeometricMatchesFilename)))
            {
                std::cerr
                        << "Cannot save computed matches in: "
                        << std::string(stlplus::folder_part(pMatchData) + "/" + sGeometricMatchesFilename);
                return false;
            }

            std::cout << "Task done in (s): " << timer.elapsed() << std::endl;

            //-- export Adjacency matrix
            std::cout << "\n Export Adjacency Matrix of the pairwise's geometric matches"
                      << std::endl;
            PairWiseMatchingToAdjacencyMatrixSVG(feature.size(),
                                                 map_GeometricMatches,
                                                 stlplus::create_filespec(stlplus::folder_part(pMatchData), "GeometricAdjacencyMatrix", "svg"));

            //-- export view pair graph once geometric filter have been done
            {
                std::set<openMVG::IndexT> set_ViewIds;
                std::transform(sfm_data.GetViews().begin(), sfm_data.GetViews().end(),
                               std::inserter(set_ViewIds, set_ViewIds.begin()), stl::RetrieveKey());
                openMVG::graph::indexedGraph putativeGraph(set_ViewIds, getPairs(map_GeometricMatches));
                openMVG::graph::exportToGraphvizData(
                        stlplus::create_filespec(stlplus::folder_part(pMatchData), "geometric_matches"),
                        putativeGraph);
            }
        }
    }
    return 0;
}

UAVErr UAVProcessFeatureSIFT::UAVProcessFeatExtract()
{
     //loop
    UAVErr err=0;

#pragma omp parallel for
    for(int i=0;i<feature.size();i++)
    {
        const std::string image = feature[i]._image_in_,
                feats = feature[i]._feature_out_,
                descs = feature[i]._descs_out_;

        std::unique_ptr<openMVG::features::Image_describer> image_describer;
        image_describer.reset(new openMVG::features::SIFT_Image_describer(openMVG::features::SIFT_Image_describer::Params(), true));

        if (!image_describer) 
        {
            std::cerr << "Cannot create the designed Image_describer:"
                      << "SIFT" << "." << std::endl;
            continue;
        }

        openMVG::image::Image<unsigned char> imageGray;
        if (!openMVG::image::ReadImage(image.c_str(), &imageGray))
            continue;

        openMVG::image::Image<unsigned char> *mask = nullptr;
        // Compute features and descriptors and export them to files
        auto regions = image_describer->Describe(imageGray, mask);
        image_describer->Save(regions.get(), feats, descs);

        regions.release();
        image_describer.release();
    }
    
    if(err==0)
        return 0;
    else
        return 5;
}

unique_ptr<openMVG::sfm::Features_Provider> UAVProcessFeatureSIFT::UAVProcessFeatsProvide()
{
    if(feature.size()==0)
        return nullptr;

    std::unique_ptr<openMVG::sfm::Features_Provider> featprovide_ptr;
    featprovide_ptr.reset(new Features_Provider_Cpu());

    bool res=true;
    for(auto iter:feature)
    {
        res=res&&((Features_Provider_Cpu*)featprovide_ptr.get())->load_pre(iter.first,iter.second._feature_out_);
    }
    return featprovide_ptr;
}


UAVErr UAVProcessFeatureSIFTGpu::UAVProcessFeatExtract()
{
    //check the number of the view
    if(feature.size()<=0)
        return 5;

    std::unique_ptr<SiftGPU>sift;
    sift.reset(new SiftGPU);

    if (sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED)
        return 5;

    //loop
    UAVErr err=0;
    for(auto iter:feature) 
    {
        const std::string image = iter.second._image_in_,
                feats = iter.second._feature_out_,
                descs = iter.second._descs_out_;

        vector<float> descriptor(1);
        vector<SiftGPU::SiftKeypoint> keys(1);

        if (sift->RunSIFT(image.c_str())) 
        {
            //get feature count
            int num1 = sift->GetFeatureNum();
            //allocate memory
            keys.resize(num1);
            descriptor.resize(128 * num1);
            sift->GetFeatureVector(&keys[0], &descriptor[0]);
            //this can be used to write your own sift file.
            //将特征点和特征描述写入文件中
            err=err|UAVProcessExportFeatsToFile(feats, descs, keys, descriptor);
        }
    }
    if(err==0)
        return 0;
    else
        return 5;
}

UAVErr UAVProcessFeatureSIFTGpu::UAVProcessExportFeatsToFile(std::string pathfeats,std::string pathdesc,
                                                             std::vector<SiftKeypoint> feats, std::vector<float> desc)
{
    string feats_path=pathfeats;
    string descs_path=pathdesc;

    ofstream ofs_feats(feats_path,ios_base::binary);
    ofstream ofs_descs(descs_path,ios_base::binary);

    if(!ofs_feats.is_open()||!ofs_descs.is_open())
        return false;
    int num = feats.size();
    ofs_feats.write((const char*)&num,sizeof(int));
    ofs_descs.write((const char*)&num,sizeof(int));
    for (int i = 0; i < num; ++i) {
        float x = feats[i].x;
        float y = feats[i].y;
        float descf[128];
        for(int j=0;j<128;++j)
        {
            descf[j]=desc[128*i+j];
        }
        ofs_feats.write((const char*)&x,sizeof(float));
        ofs_feats.write((const char*)&y,sizeof(float));
        ofs_descs.write((const char*)descf,sizeof(float)*128);
    }
    ofs_feats.close();
    ofs_descs.close();
    return 0;
}


bool UAVProcessFeatureSIFTGpu::UAVImportFeatsToFile(string pathdesc, vector<float> &desc)
{
    ifstream ifs(pathdesc,ios_base::binary|ios_base::in);
    if(!ifs.is_open())
        return false;

    int num;
    ifs.read((char*)&num,sizeof(int));
    try
    {
        float* descf=new float[128*num];
        ifs.read((char*)descf,sizeof(float)*num*128);

        desc.resize(128*num);
        for (int i = 0; i <128*num ; ++i) {
            desc[i]=descf[i];
        }
        delete[]descf;descf=NULL;
        return true;
    }catch (std::bad_alloc e)
    {
        printf(e.what());
        return false;
    }
}

bool UAVProcessFeatureSIFTGpu::UAVExportMatchesToFile(string path,int srcImg,int desImg,int matchnum,int (*matches)[2])
{
    ofstream ofs(path,ios_base::app);
    ofs<<srcImg<<"  "<<desImg<<"  "<<matchnum<<endl;
    for(int i=0;i<matchnum;++i){
        ofs<<matches[i][0]<<"  "<<matches[i][1]<<endl;
    }
    return true;
}

/// Display pair wises matches as an Adjacency matrix in svg format
void PairMatchingToAdjacencyMatrixSVG
        (
                const size_t NbImages,
                const MatchesList & map_Matches,
                const std::string & sOutName
        )
{
    if ( !map_Matches.empty())
    {
        const float scaleFactor = 5.0f;
        svg::svgDrawer svgStream((NbImages+3)*5, (NbImages+3)*5);
        // Go along all possible pair
        for (size_t I = 0; I < NbImages; ++I) {
            for (size_t J = 0; J < NbImages; ++J) {
                // If the pair have matches display a blue boxes at I,J position.
                auto iterSearch = map_Matches.find(std::make_pair(I,J));
                if (iterSearch != map_Matches.end())
                {
                    // Display as a tooltip: (IndexI, IndexJ NbMatches)
                    std::ostringstream os;
                    svgStream.drawSquare(J*scaleFactor, I*scaleFactor, scaleFactor/2.0f,
                                         svg::svgStyle().fill("blue").noStroke());
                } // HINT : THINK ABOUT OPACITY [0.4 -> 1.0] TO EXPRESS MATCH COUNT
            }
        }
        // Display axes with 0 -> NbImages annotation : _|
        std::ostringstream osNbImages;
        osNbImages << NbImages;
        svgStream.drawText((NbImages+1)*scaleFactor, scaleFactor, scaleFactor, "0", "black");
        svgStream.drawText((NbImages+1)*scaleFactor,
                           (NbImages)*scaleFactor - scaleFactor, scaleFactor, osNbImages.str(), "black");
        svgStream.drawLine((NbImages+1)*scaleFactor, 2*scaleFactor,
                           (NbImages+1)*scaleFactor, (NbImages)*scaleFactor - 2*scaleFactor,
                           svg::svgStyle().stroke("black", 1.0));

        svgStream.drawText(scaleFactor, (NbImages+1)*scaleFactor, scaleFactor, "0", "black");
        svgStream.drawText((NbImages)*scaleFactor - scaleFactor,
                           (NbImages+1)*scaleFactor, scaleFactor, osNbImages.str(), "black");
        svgStream.drawLine(2*scaleFactor, (NbImages+1)*scaleFactor,
                           (NbImages)*scaleFactor - 2*scaleFactor, (NbImages+1)*scaleFactor,
                           svg::svgStyle().stroke("black", 1.0));

        std::ofstream svgFileStream( sOutName.c_str());
        svgFileStream << svgStream.closeSvgFile().str();
    }
}
UAVErr UAVProcessFeatureSIFTGpu::UAVProcessMatchesExtract(std::string pMatchList,std::string pMatchData)
{
    SiftMatchGPU *matcher = new SiftMatchGPU(4096);
    SiftGPU *sift = new SiftGPU;
    if (sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED)
        return false;
    matcher->VerifyContextGL();

    int iMatchingVideoMode = -1;

    openMVG::sfm::SfM_Data sfm_data;
    if (!Load(sfm_data, sfm_data_path_, openMVG::sfm::ESfM_Data(openMVG::sfm::VIEWS|openMVG::sfm::INTRINSICS))) {
        std::cerr << std::endl
                  << "The input SfM_Data file \""<< sfm_data_path_ << "\" cannot be read." << std::endl;
        return false;
    }

    openMVG::system::Timer timer;
    {
        MatchesList pairs;
        MatchesList pairs_filter;
        if(!openMVG::loadPairs(feature.size(), pMatchList, pairs))
        {
            return 5;
        }
        int matchpairs=pairs.size();
        C_Progress_display my_progress_bar( matchpairs,
                                            std::cout, "\n- EXTRACT Matches -\n" );
        for ( const auto & cur_pair : pairs )
        {
            int img1 = cur_pair.first, img2 = cur_pair.second;

            //load feats and describes;
            const std::string sDesc1 = feature[img1]._descs_out_,
                                sDesc2 = feature[img2]._descs_out_;

            std::vector<float> desc1,desc2;
            if(!UAVImportFeatsToFile(sDesc1,desc1)||!UAVImportFeatsToFile(sDesc2,desc2))
            {
                continue;
            }
            int num1 = desc1.size()/128;
            int num2 = desc2.size()/128;
            //matches
            matcher->SetDescriptors(0, num1, &desc1[0]); //image 1
            matcher->SetDescriptors(1, num2, &desc2[0]); //image 2
            int (*match_buf)[2] = new int[num1][2];
            int num_match = matcher->GetSiftMatch(num1, match_buf);
            if(num_match>10)
            {
                UAVExportMatchesToFile(pMatchData,img1,img2,num_match,match_buf);
                pairs_filter.insert(std::make_pair(img1,img2));
            }
            ++my_progress_bar;
        }


        PairMatchingToAdjacencyMatrixSVG(sfm_data.GetViews().size(),
                                         pairs_filter,
                                         stlplus::create_filespec(stlplus::folder_part(pMatchData), "GeometricAdjacencyMatrix", "svg"));


        //-- export view pair graph once geometric filter have been done
        {
            std::set< openMVG::IndexT> set_ViewIds;
            std::transform(sfm_data.GetViews().begin(), sfm_data.GetViews().end(),
                           std::inserter(set_ViewIds, set_ViewIds.begin()), stl::RetrieveKey());
            openMVG::graph::indexedGraph putativeGraph(set_ViewIds, pairs_filter);
            openMVG::graph::exportToGraphvizData(
                    stlplus::create_filespec(stlplus::folder_part(pMatchData), "GeometricAdjacencyMatrix"),
                    putativeGraph);
        }
    }
}

unique_ptr<openMVG::sfm::Features_Provider> UAVProcessFeatureSIFTGpu::UAVProcessFeatsProvide(){
    if(feature.size()==0)
        return nullptr;

    std::unique_ptr<openMVG::sfm::Features_Provider> featprovide_ptr;
    featprovide_ptr.reset(new Features_Provider_Gpu());

    bool res=true;
    for(auto iter:feature){
        res=res&&((Features_Provider_Gpu*)featprovide_ptr.get())->load_pre(iter.first,iter.second._feature_out_);
    }
    if(res)
        return featprovide_ptr;
    else
        return nullptr;
}
