/*
 * 2018-7-6 : by Lingsheng Kong
 */


#include <v3d_interface.h>
#include "v3d_message.h"
#include "openSWCDialog.h"
#include "profile_swc.h"
#include <vector>
#include <iostream>
//#include "eswc_core.h"
#include <math.h>
#include <numeric>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>
#include "v3d_message.h"
#include "compute_tubularity.h"

#include "neuron_image_snr_plugin.h"

using namespace std;

const QString title = QObject::tr("Image Profile with SWC ROI");

#ifndef MIN
#define MIN(a, b)  ( ((a)<(b))? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a, b)  ( ((a)>(b))? (a) : (b) )
#endif

#ifndef dist
#define dist(a,b) sqrt(((a).x-(b).x)*((a).x-(b).x)+((a).y-(b).y)*((a).y-(b).y)+((a).z-(b).z)*((a).z-(b).z))
#endif

static void cutoff_outliers(vector<double> & x, float cut_off_ratio)
{
    //remove the top and bottom 10% data, to be more robust
    sort(x.begin(), x.end());
    int num_to_remove = x.size()*cut_off_ratio;

    // erase the top and bottom N elements:
    x.erase( x.begin(), x.begin()+ num_to_remove);
    x.erase( x.end()-num_to_remove, x.end());
}

static V3DLONG boundValue(V3DLONG x, V3DLONG m_min, V3DLONG m_max)
{
    x = MAX(x, m_min);
    x = MIN(x, m_max);
    return x;

}

static double median(vector<double> x)
{
    sort(x.begin(), x.end());
    return  x[x.size()/2];
}

static double mean(vector<double> x)
{
     double x_mean  = accumulate( x.begin(), x.end(), 0.0 )/ x.size();
     return x_mean;
}


static double standard_dev(vector<double> x)
{
    double x_mean  = mean(x);

    double sum2 = 0;
    for ( V3DLONG i = 0; i < x.size(); i++ )
    {
        sum2 += pow(x[i]-x_mean,2);
    }
    double x_std = sqrt(sum2/x.size());
    return x_std;
}



static ENSEMBLE_METRICS stats_ensemble(QList<IMAGE_METRICS> result_metrics,double cut_off_ratio)
{  // aggregate all segment stats into one stats
    ENSEMBLE_METRICS stats; // average stats over all segments
    vector <double> cnrs;
    vector <double> dys;
    vector <double> tubus;
    vector <double> bgs;
    vector <double> fgs;



    for (int i  = 0; i < result_metrics.size() ; i++)
    {
        cnrs.push_back( result_metrics[i].cnr);
        dys.push_back(result_metrics[i].dy);
        tubus.push_back(result_metrics[i].tubularity_mean);
        bgs.push_back(result_metrics[i].bg_mean);
        fgs.push_back(result_metrics[i].fg_mean);
    }


    cutoff_outliers(cnrs,cut_off_ratio);
    cutoff_outliers(dys,cut_off_ratio);
    cutoff_outliers(tubus,cut_off_ratio);
    cutoff_outliers(bgs,cut_off_ratio);
    cutoff_outliers(fgs,cut_off_ratio);


    stats.mean_cnr = mean(cnrs);
    stats.mean_dy = mean(dys);
    stats.mean_tubularity = mean(tubus);
    stats.mean_bg = mean(bgs);
    stats.mean_fg = mean(fgs);


    stats.std_bg = standard_dev(bgs);
    stats.std_fg = standard_dev(fgs);
    stats.std_dy = standard_dev(dys);
    stats.std_cnr = standard_dev(cnrs);
    stats.std_tubularity = standard_dev(tubus);

    return stats;

}






//flip image along the Y direction ( due to the image matrix order convention in Vaa3D)
bool flip_y (Image4DSimple * image)
{
    unsigned char * data1d = image->getRawData();
    V3DLONG in_sz[3];
    in_sz[0] =image->getXDim();
    in_sz[1] = image->getYDim();
    in_sz[2] = image->getZDim();

    V3DLONG hsz1 = floor((double) (in_sz[1]-1)/2.0);
    if (hsz1*2<in_sz[1]-1)
        hsz1+=1;

    for (int j=0;j<hsz1;j++)
        for (int i=0;i<in_sz[0];i++)
        {
            unsigned char tmpv = data1d[(in_sz[1]-j-1)*in_sz[0] + i];
            data1d[(in_sz[1]-j-1)*in_sz[0] + i] = data1d[j*in_sz[0] + i];
            data1d[j*in_sz[0] + i] = tmpv;
        }

    return true;
}




#define MAX_INTENSITY 255
bool invert_intensity (Image4DSimple * image)
{
    unsigned char * data1d = image->getRawData();
    V3DLONG in_sz[3];
    in_sz[0] = image->getXDim();
    in_sz[1] = image->getYDim();
    in_sz[2] = image->getZDim();

    for (V3DLONG z = 0; z < in_sz[2]; z++)
    {
        for ( V3DLONG y = 0; y < in_sz[1]; y++)
        {
            for ( V3DLONG x = 0; x < in_sz[0]; x++)
            {
                V3DLONG index_1d = z * (image->getXDim() * image->getYDim())  + y * image->getXDim() + x;
                data1d[index_1d] = MAX_INTENSITY - data1d[index_1d] ;
            }
        }
    }

    return true;
}


bool writeMetrics2CSV(QList<IMAGE_METRICS> result_metrics, QString output_csv_file)
{
    QFile file(output_csv_file);
    if (!file.open(QFile::WriteOnly|QFile::Truncate))
    {
        cout <<"Error opening the file "<<output_csv_file.toStdString().c_str() << endl;
        return false;
    }
    else
    {
        QTextStream stream (&file);
        stream<< "segment_type,num_of_nodes,dynamic_range,cnr,snr,tubularity_mean,tubularity_std,fg_mean,fg_std,bg_mean,bg_std,vr/3d"<<"\n";

        for (int i  = 0; i < result_metrics.size() ; i++)
        {
            stream << result_metrics[i].type       <<","
                   << result_metrics[i].num_of_nodes <<","
                   << result_metrics[i].dy         <<","
                   << result_metrics[i].cnr        <<","
                   << result_metrics[i].snr        <<","
                   << result_metrics[i].tubularity_mean <<","
                   << result_metrics[i].tubularity_std <<","
                   << result_metrics[i].fg_mean    <<","
                   << result_metrics[i].fg_std     <<","
                   << result_metrics[i].bg_mean    <<","
                   << result_metrics[i].bg_std     <<"\n";
        }
    file.close();
    }
    return true;

}





bool  profile_swc_func(V3DPluginCallback2 &callback, const V3DPluginArgList & input, V3DPluginArgList & output)
{
    vector<char*> infiles, inparas, outfiles;
    if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
    if(input.size() >= 2) inparas = *((vector<char*> *)input.at(1).p);
    if(output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

    if(infiles.size() != 2 && infiles.size() != 3)
    {
        cerr<<"Invalid input"<<endl;
        return false;
    }
    QString imageFileName = QString(infiles[0]);
    QString swcFileName = QString(infiles[1]);
    QString output_csv_file;
    if(!outfiles.empty())
        output_csv_file = QString(outfiles[0]);
    else
        output_csv_file = swcFileName + ".csv";

    float  dilate_ratio = (inparas.size() >= 1) ? atof(inparas[0]) : 3.0;
    int  flip = (inparas.size() >= 2) ? atoi(inparas[1]) : 1;
    int  invert = (inparas.size() >= 3) ? atoi(inparas[2]) : 1;
    float  cut_off_ratio = (inparas.size() >= 4) ? atof(inparas[3]) : 0.05;
    cout<<"inimg_file = "<< imageFileName.toStdString()<<endl;
    cout<<"inswc_file = "<< swcFileName.toStdString()<<endl;
    cout<<"output_file = "<< output_csv_file.toStdString()<<endl;
    cout<<"dilate_ratio = "<< dilate_ratio<<endl;
    cout<<"flip y = "<< flip <<endl;
    cout<<"invert intensity = "<< invert <<endl;
    cout<<"cut_off_ratio = "<< cut_off_ratio <<endl;
    NeuronTree  neuronTree;

    if (swcFileName.endsWith(".swc") || swcFileName.endsWith(".SWC"))
    {
        neuronTree = readSWC_file(swcFileName);
    }
    else
    {
        cout<<"The file type you specified is not supported."<<endl;
        return false;
    }

    Image4DSimple *image = callback.loadImage((char * )imageFileName.toStdString().c_str());

    if (! image->convert_to_UINT8()){
        cout<< "Error in converting data into  UINT8 type."<<endl;
        return false;
    }

  

	return true;

}

/*Calculating the distance from a point to a straight line segment*/
static float distancePtSeg(const float* pt, const NeuronSWC p, const NeuronSWC q)
{
	float pqx = q.x - p.x;
	float pqy = q.y - p.y;
	float pqz = q.z - p.z;
	float dx = pt[0] - p.x;
	float dy = pt[1] - p.y;
	float dz = pt[2] - p.z;
	float d = pqx*pqx + pqy*pqy + pqz*pqz;      
	float t = pqx*dx + pqy*dy + pqz*dz;         
	if (d > 0)                            
		t /= d;    
	if (t < 0)
		t = 0;     
	else if (t > 1)
		t = 1;     
 
	
	dx = p.x + t*pqx - pt[0];
	dy = p.y + t*pqy - pt[1];
	dz = p.z + t*pqz - pt[2];
	return sqrt(dx*dx + dy*dy + dz*dz);
}

IMAGE_METRICS   compute_metrics(Image4DSimple *image,int num,  QList<NeuronSWC> neuronSegment, float dilate_ratio, float cut_off_ratio,V3DPluginCallback2 &callback)
{
	//cout<<"compute begin"<<endl;
	//cout<<neuronSegment.size()<<endl;
    IMAGE_METRICS metrics;
    metrics.type = neuronSegment.at(0).type;
    metrics.num_of_nodes = neuronSegment.size();
	metrics.length = 0.0;
    metrics.cnr = 0.0;
    metrics.snr = 0.0;
    metrics.dy = 0.0;
    metrics.tubularity_mean = 0.0;
    metrics.fg_mean = 0.0;
    metrics.bg_mean = 0.0;
    metrics.tubularity_std = 0.0;
    metrics.fg_std = 0.0;
    metrics.bg_std = 0.0;
	metrics.vr = 0;
	metrics.number = num;
	double distance = 0.0;
	/*if(neuronSegment.size()<2)
		metrics.length = 1.0;
	else{
		for(int i=0;i<neuronSegment.size()-1;i++){
			distance = distance + sqrt(pow((neuronSegment.at(i+1).x*0.2-neuronSegment.at(i).x*0.2),2)+pow((neuronSegment.at(i+1).y*0.2-neuronSegment.at(i).y*0.2),2)+pow((neuronSegment.at(i+1).z-neuronSegment.at(i).z),2))/1000;
		}
		metrics.length = distance;
	}*/
	for(int i=0;i<neuronSegment.size()-1;i++){
		distance = distance + sqrt(pow((neuronSegment.at(i+1).x*0.2-neuronSegment.at(i).x*0.2),2)+pow((neuronSegment.at(i+1).y*0.2-neuronSegment.at(i).y*0.2),2)+pow((neuronSegment.at(i+1).z-neuronSegment.at(i).z),2))/1000;
	}
	metrics.length = distance;

	if((fabs(neuronSegment.at(0).radius - 0.618f) < 0.001f)||(fabs(neuronSegment.at(0).radius - 0.666f) < 0.001f)){
		metrics.vr = 1;
	}

    V3DLONG min_x = image->getXDim() , min_y = image->getYDim() ,  min_z = image->getZDim() , max_x = 0, max_y = 0, max_z= 0;

    //get the bounding box of ROI
    for (V3DLONG i =0 ; i < neuronSegment.size() ; i++)
    {
       NeuronSWC node = neuronSegment.at(i);
       float r;
       if (node.radius < 0){
           r = 1;
           cout <<" warning: node radius is negtive?! " <<endl;
       }
       else{
           r = node.radius;
       }

	   //cout << r << endl;

       float dilate_radius = dilate_ratio * r;
       V3DLONG node_x_min = node.x - r - dilate_radius + 0.5; // with rounding
       V3DLONG node_y_min = node.y - r - dilate_radius + 0.5;
       V3DLONG node_z_min = node.z - r - dilate_radius + 0.5;

       V3DLONG node_x_max = node.x + r + dilate_radius + 0.5;
       V3DLONG node_y_max = node.y + r + dilate_radius + 0.5;
       V3DLONG node_z_max = node.z + r + dilate_radius + 0.5;

       if(min_x > node_x_min) min_x = node_x_min;
       if(min_y > node_y_min) min_y = node_y_min;
       if(min_z > node_z_min) min_z = node_z_min;
       if(max_x < node_x_max) max_x = node_x_max;
       if(max_y < node_y_max) max_y = node_y_max;
       if(max_z < node_z_max) max_z = node_z_max;
    }

	//cout<<"22"<<endl;
	//cout<<neuronSegment.size()<<endl;
    min_x = boundValue(min_x, 0,image->getXDim()-1 );
    min_y = boundValue(min_y, 0,image->getYDim()-1 );
    min_z = boundValue(min_z, 0,image->getZDim()-1 );

    max_x = boundValue(max_x, 0,image->getXDim()-1 );
    max_y = boundValue(max_y, 0,image->getYDim()-1 );
    max_z = boundValue(max_z, 0,image->getZDim()-1 );


    int width =  max_x - min_x + 1;
    int height = max_y - min_y + 1;
    int depth =  max_z - min_z + 1;

	//cout << "ROI dimension: " << width << " x " << height << " x " << depth << endl;


    if (image->getZDim() == 1)
    {
        depth = 1;
        min_z = 0;
        max_z = 0;
    }

    int size_1d = width * height *depth;

    //cout << "min: "<< min_x<<" "<< min_y <<" " <<min_z<<endl;
    //cout << "max: "<< max_x<<" "<< max_y <<" " <<max_z<<endl;
    //cout << "size:" << width<<" x" <<height <<" x"<<depth<<endl;


    vector <double> fg_1d;
    vector <double> bg_1d;

    unsigned char  * roi_1d_visited = new  unsigned char [size_1d];
    int FG = 255;
    int BG = 100;
    int FUZZY = 10;



    for (V3DLONG i = 0; i < size_1d ; i++)
    {
        roi_1d_visited[i] = 0;  //unvisited tag
    }


    vector <double> tubularities;
	if(neuronSegment.size()==1){
		for (V3DLONG i = 0; i < neuronSegment.size() ; i++){
			NeuronSWC node = neuronSegment.at(i);
        float r;
        if (node.radius < 0 ){
            r = 1;
        }
        else
        {
            r = node.radius;
        }

        float dilate_radius = dilate_ratio * r;

        V3DLONG xb,xe,yb,ye,zb,ze;

        // for each node
        //label foreground
        xb = boundValue(node.x - r + 0.5, 0,image->getXDim()-1 );
        xe = boundValue(node.x + r + 0.5, 0,image->getXDim()-1 );
        yb = boundValue(node.y - r + 0.5, 0,image->getYDim()-1 );
        ye = boundValue(node.y + r + 0.5, 0,image->getYDim()-1 );
        zb = boundValue(node.z - r + 0.5, 0,image->getZDim()-1 );
        ze = boundValue(node.z + r + 0.5, 0,image->getZDim()-1 );
        for (V3DLONG z = zb; z <= ze; z++)
        {
            for ( V3DLONG y = yb; y <= ye; y++)
            {
                for ( V3DLONG x = xb; x <= xe; x++)
                {
                    V3DLONG index_1d = z * (image->getXDim() * image->getYDim())  + y * image->getXDim() + x;
                    V3DLONG roi_index =  (z - min_z) * (width * height)  + (y - min_y) * width + (x - min_x);
                    if  ( roi_1d_visited[roi_index] != FG )
                    {
                        roi_1d_visited[roi_index] = FG;
                    }
                }

            }

        }
		//cout<<"label done!"<<endl;
        // compute tubularity for each node
        V3DLONG xx = boundValue(node.x, 0,image->getXDim()-1 );
        V3DLONG yy = boundValue(node.y, 0,image->getYDim()-1 );
        V3DLONG zz = boundValue(node.z, 0,image->getZDim()-1 );
        double tubuV = compute_anisotropy_sphere(image->getRawData(), image->getXDim(), image->getYDim(), image->getZDim(), 0, xx,yy,zz, r + dilate_radius);
		//cout << tubuV << " ";
        tubularities.push_back(tubuV);
		}

	}else{
		for (V3DLONG i = 0; i < neuronSegment.size()-1 ; i++)
    {
		NeuronSWC node = neuronSegment.at(i),node2 = neuronSegment.at(i+1);
        float r;
        if (node.radius < 0 ){
            r = 1;
        }
        else
        {
            r = node.radius;
        }

        float dilate_radius = dilate_ratio * r;

        V3DLONG xb,xe,yb,ye,zb,ze,xb2,xe2,yb2,ye2,zb2,ze2;

        // for each two neighber nodes
        //label foreground
        xb = boundValue(node.x - r + 0.5, 0,image->getXDim()-1 );
        xe = boundValue(node.x + r + 0.5, 0,image->getXDim()-1 );
        yb = boundValue(node.y - r + 0.5, 0,image->getYDim()-1 );
        ye = boundValue(node.y + r + 0.5, 0,image->getYDim()-1 );
        zb = boundValue(node.z - r + 0.5, 0,image->getZDim()-1 );
        ze = boundValue(node.z + r + 0.5, 0,image->getZDim()-1 );
		xb2 = boundValue(node2.x - r + 0.5, 0,image->getXDim()-1 );
        xe2 = boundValue(node2.x + r + 0.5, 0,image->getXDim()-1 );
        yb2 = boundValue(node2.y - r + 0.5, 0,image->getYDim()-1 );
        ye2 = boundValue(node2.y + r + 0.5, 0,image->getYDim()-1 );
        zb2 = boundValue(node2.z - r + 0.5, 0,image->getZDim()-1 );
        ze2 = boundValue(node2.z + r + 0.5, 0,image->getZDim()-1 );
        for (V3DLONG z = min(zb,zb2); z <= max(ze,ze2); z++)
        {
            for ( V3DLONG y = min(yb,yb2); y <= max(ye,ye2); y++)
            {
                for ( V3DLONG x = min(xb,xb2); x <= max(xe,xe2); x++)
                {
					float f[3] = {x,y,z};
                    V3DLONG index_1d = z * (image->getXDim() * image->getYDim())  + y * image->getXDim() + x;
                    V3DLONG roi_index =  (z - min_z) * (width * height)  + (y - min_y) * width + (x - min_x);
                    if  ((roi_1d_visited[roi_index] != FG )&&(distancePtSeg(f,node,node2)- (2) <= 0.0001f))
                    {
                        roi_1d_visited[roi_index] = FG;
                    }
                }

            }

        }

		for (V3DLONG z = min(zb,zb2); z <= max(ze,ze2); z++)
        {
            for ( V3DLONG y = min(yb,yb2); y <= max(ye,ye2); y++)
            {
                for ( V3DLONG x = min(xb,xb2); x <= max(xe,xe2); x++)
                {
					float f[3] = {x,y,z};
                    V3DLONG index_1d = z * (image->getXDim() * image->getYDim())  + y * image->getXDim() + x;
                    V3DLONG roi_index =  (z - min_z) * (width * height)  + (y - min_y) * width + (x - min_x);
                    if  ((roi_1d_visited[roi_index] != FG ))
                    {
                        roi_1d_visited[roi_index] = BG;
                    }
                }

            }

        }

		//cout<<"label done!"<<endl;
        // compute tubularity for each node
        V3DLONG xx = boundValue(node.x, 0,image->getXDim()-1 );
        V3DLONG yy = boundValue(node.y, 0,image->getYDim()-1 );
        V3DLONG zz = boundValue(node.z, 0,image->getZDim()-1 );
        double tubuV = compute_anisotropy_sphere(image->getRawData(), image->getXDim(), image->getYDim(), image->getZDim(), 0, xx,yy,zz, r + dilate_radius);
		//cout << tubuV << " ";
        tubularities.push_back(tubuV);
    }
	}
    

    //collect labled pixel data into bg and fg vectors
    for (V3DLONG i = 0; i < size_1d ; i++)
    {
        V3DLONG roi_index = i;

        V3DLONG z = roi_index/(width*height);
        V3DLONG y = (roi_index - z* (width*height) )/width;
        V3DLONG x = roi_index - z* (width*height)  - y*width;
        z += min_z;
        y += min_y;
        x += min_x;
        V3DLONG index_1d =  z * (image->getXDim() * image->getYDim())  + y * image->getXDim() + x;

        if (roi_1d_visited[roi_index] == FG ){
            fg_1d.push_back(double(image->getRawData()[index_1d]));
        }
        if (roi_1d_visited[roi_index] != FG ){
            bg_1d.push_back(double(image->getRawData()[index_1d]));
        }

    }

    // compute metrics
    // remove the top and bottom 5% data to be robust
    cutoff_outliers(fg_1d,cut_off_ratio);
    cutoff_outliers(bg_1d,cut_off_ratio);
    cutoff_outliers(tubularities,cut_off_ratio);

    double max_fg =  *( max_element(fg_1d.begin(), fg_1d.end()));
    double min_fg =  * (min_element(fg_1d.begin(), fg_1d.end()));
    metrics.dy = fabs(max_fg - min_fg);

    //double bg_mean  = mean(bg_1d);
	sort(bg_1d.begin(),bg_1d.end());
	int a = (int)(bg_1d.size()*0.2);
	double sum = 0;
	cout<<"bg size: "<<a<<endl;
	if(bg_1d.size()==1)
		sum = sum + bg_1d[0];
	else{
		for(int i = bg_1d.size()-1;i>bg_1d.size()-a-1;i--){
		sum = sum+bg_1d[i];
	}
	}
	
	//cout<<a<<endl;
	//cout<<"bg size: "<<a<<endl;
	cout<<"fg size: "<<fg_1d.size()<<endl;
	//double bg_mean  = mean(bg_1d);
    double fg_mean  = mean(fg_1d);
	double bg_mean  = sum/(a+1);


    metrics.bg_std = standard_dev(bg_1d);
    metrics.fg_std = standard_dev(fg_1d);
    metrics.tubularity_std  = standard_dev(tubularities);
    

    //metrics.snr = fabs(fg_mean - bg_mean+0.001)/(metrics.fg_std+0.001); //avoid denominator=0
	metrics.snr = fabs(fg_mean/(bg_mean+0.001));
    metrics.cnr = fabs(fg_mean - bg_mean+0.001)/(metrics.bg_std+0.001);

    //average tubularity
     metrics.tubularity_mean = mean(tubularities);
     metrics.bg_mean = bg_mean;
     metrics.fg_mean = fg_mean;


    return metrics;

}

QList<IMAGE_METRICS> intensity_profile(NeuronTree neuronTree, int num,Image4DSimple * image, float dilate_ratio, int flip, int invert, float cut_off_ratio, V3DPluginCallback2 &callback)
{
	//cout<<"test1"<<endl;
    if(flip > 0)
    {
      flip_y(image);
      cout<<"warning: the image is flipped in Y by default, to be consistent with other image readers, e.g. ImageJ."<<endl;
    }

    if (invert > 0)
    {
        invert_intensity(image);
        cout<<"warning: the image is flipped in Y by default, to be consistent with other image readers, e.g. ImageJ."<<endl;
    }


    QList<NeuronSWC> neuronSWCs =  neuronTree.listNeuron;

    QList<IMAGE_METRICS> result_metrics;
	//cout<<"test2"<<endl;
    // all types
    IMAGE_METRICS metrics = compute_metrics( image,num, neuronSWCs,dilate_ratio, cut_off_ratio,callback );
	//cout<<"compute done!"<<endl;
    metrics.type = -1;  //all types
    result_metrics.push_back(metrics);

    // pool neuron nodes by segment types

    QList<NeuronSWC> neuronSWC_sameType;
    QList <QList<NeuronSWC> > neuronSWC_lists;
    std::map<int,int> mapTypeToId;
    int pre_type = neuronSWCs[0].type;
    int count = 0;//numer fo different segment types
    int current_type = pre_type;
    for (V3DLONG i = 0 ; i < neuronSWCs.size() ; i++)
    {
        current_type = neuronSWCs[i].type;



        if (current_type != pre_type)
        {// change if the type, need to push the previous continues types into the table
            if (!neuronSWC_sameType.isEmpty())
            {
                if ( mapTypeToId.count(pre_type) == 0   )
                {// and the type is new
                    neuronSWC_lists.push_back(neuronSWC_sameType);
                    mapTypeToId[pre_type] = count;
                    //cout<<"map "<<pre_type<<" to "<<count<<endl;
                    count++;

                }
                else
                { // type exists, directly append
                    int jj = mapTypeToId[pre_type];
                    neuronSWC_lists[jj].append(neuronSWC_sameType);;
                }

                neuronSWC_sameType.clear();
            }


            pre_type = current_type;
        }

        neuronSWC_sameType.push_back(neuronSWCs.at(i));

    }


    //at the end
    if (!neuronSWC_sameType.isEmpty()){
        if ( mapTypeToId.count(pre_type) == 0)
        {
            neuronSWC_lists.push_back(neuronSWC_sameType);
            mapTypeToId[current_type] = count;
           // cout<<"last: map "<<current_type<<" to "<<count<<endl;
            count++ ;
        }
        else
        {
            int jj = mapTypeToId[current_type];
            neuronSWC_lists[jj].append(neuronSWC_sameType);;
        }
    }

    //collect metrics
    cout<< "Profile " << count<< " different segment types" <<endl;
    for (int j = 0; j < count; j++)
    {
		cout<<neuronSWC_lists.size()<<endl;
        if (!neuronSWC_lists[j].isEmpty() )
        {
			//cout<<"000"<<endl;
            IMAGE_METRICS metrics = compute_metrics( image,num, neuronSWC_lists[j] ,dilate_ratio,cut_off_ratio,callback );
			//cout<<"111"<<endl;
            result_metrics.push_back(metrics);
			//cout<<"222"<<endl;
        }

    }
	cout<< "Profile done" <<endl;

    return result_metrics;
}

void printHelp(const V3DPluginCallback2 &callback, QWidget *parent)
{
	v3d_msg("This plugin is used for profiling images with SWC specified ROIs.");
}

void printHelp(const V3DPluginArgList & input, V3DPluginArgList & output)
{
    cout<<"This plugin is used for profiling 2D images with SWC specified ROIs.\n";
    cout<<"usage:\n";
    cout<<"v3d -x neuron_image_profiling -f profile_swc -i <inimg_file> <inswc_file> -o <out_file> -p <dilation_ratio>  <flip> <cut_off_ratio> \n";
    cout<<"For example :\n";
    cout<<"v3d -x neuron_image_profiling -f profile_swc -i a.v3dpbd a.swc -o profile.csv -p 3.0 0 0 0.05\n";
    cout<<"inimg_file:\t\t input image file\n";
    cout<<"inswc_file:\t\t input .swc file\n";
    cout<<"out_file:\t\t (not required) output statistics of intensities into a csv file. DEFAUTL: '<inswc_file>.csv'\n";
    cout<<"dilation_radius :\t (not required) the dilation ratio to expand the radius for background ROI extraction\n";
    cout<<"flip in y [0 or 1]\t (not required)\n";
    cout<<"invert intensity [0 or 1]\t (not required, signal should have higher intensties than background, for tubuliarty calculation)"<<endl;
    cout<<"cutoff ratio[default 0.05]\t (not required) to make the stats more robust due to ROI errors, you can specify the ratio to remove the top and bottom outliers.\n";
}


/*******************************************/



void findSegLowerBound(QList<NeuronSWC>* segPtr, Image4DSimple* image4DPtr, V3DLONG lBounds[])
{
	float segLowerBound[3];
	segLowerBound[0] = 10000;
	segLowerBound[1] = 10000;
	segLowerBound[2] = 10000;
	for (QList<NeuronSWC>::iterator it = segPtr->begin(); it != segPtr->end(); ++it)
	{
		if (it->x < segLowerBound[0]) segLowerBound[0] = it->x;
		if (it->y < segLowerBound[1]) segLowerBound[1] = it->y;
		if (it->z < segLowerBound[2]) segLowerBound[2] = it->z;
	}

	lBounds[0] = boundValue(segLowerBound[0], 0, image4DPtr->getXDim() - 1);
	lBounds[1] = boundValue(segLowerBound[1], 0, image4DPtr->getYDim() - 1);
	lBounds[2] = boundValue(segLowerBound[2], 0, image4DPtr->getZDim() - 1);
}
