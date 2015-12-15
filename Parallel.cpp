/*
 *  Parallel.cpp
 *  
 *
 *  Created by Amy Takayesu on 12/14/15.
 *  Copyright 2015 __MyCompanyName__. All rights reserved.
 *
 */

#include "Parallel.h"

#include "opencv2/core/core.hpp"
#include "opencv2/ml/ml.hpp"

#include <cstdio>
#include <vector>
#include <iostream>
#include <mpi.h>


using namespace std;
using namespace cv;
using namespace cv::ml;

int my_rank, num_procs;

static void help()
{
    printf("\nThe sample demonstrates how to train Random Trees classifier\n"
		   "(or Boosting classifier, or MLP, or Knearest, or Nbayes, or Support Vector Machines - see main()) using the provided dataset.\n"
		   "\n"
		   "We use the sample database letter-recognition.data\n"
		   "from UCI Repository, here is the link:\n"
		   "\n"
		   "Newman, D.J. & Hettich, S. & Blake, C.L. & Merz, C.J. (1998).\n"
		   "UCI Repository of machine learning databases\n"
		   "[http://www.ics.uci.edu/~mlearn/MLRepository.html].\n"
		   "Irvine, CA: University of California, Department of Information and Computer Science.\n"
		   "\n"
		   "The dataset consists of 20000 feature vectors along with the\n"
		   "responses - capital latin letters A..Z.\n"
		   "The first 16000 (10000 for boosting)) samples are used for training\n"
		   "and the remaining 4000 (10000 for boosting) - to test the classifier.\n"
		   "======================================================\n");
    printf("\nThis is letter recognition sample.\n"
		   "The usage: letter_recog [-data <path to letter-recognition.data>] \\\n"
		   "  [-save <output XML file for the classifier>] \\\n"
		   "  [-load <XML file with the pre-trained classifier>] \\\n"
		   "  [-boost|-mlp|-knearest|-nbayes|-svm] # to use boost/mlp/knearest/SVM classifier instead of default Random Trees\n" );
}

// This function reads data and responses from the file <filename>
static bool
read_num_class_data( const string& filename, int var_count,
					Mat* _data, Mat* _responses )
{
    const int M = 1024;
    char buf[M+2];
	
    Mat el_ptr(1, var_count, CV_32F);
    int i;
    vector<int> responses;
	
    _data->release();
    _responses->release();
	
    FILE* f = fopen( filename.c_str(), "rt" );
    if( !f )
    {
        cout << "Could not read the database " << filename << endl;
        return false;
    }
	
    for(;;)
    {
        char* ptr;
        if( !fgets( buf, M, f ) || !strchr( buf, ',' ) )
            break;
        responses.push_back((int)buf[0]);
        ptr = buf+2;
        for( i = 0; i < var_count; i++ )
        {
            int n = 0;
            sscanf( ptr, "%f%n", &el_ptr.at<float>(i), &n );
            ptr += n + 1;
        }
        if( i < var_count )
            break;
        _data->push_back(el_ptr);
    }
    fclose(f);
    Mat(responses).copyTo(*_responses);
	
    cout << "The database " << filename << " is loaded.\n";
	
    return true;
}

template<typename T>
static Ptr<T> load_classifier(const string& filename_to_load)
{
    // load classifier from the specified file
    Ptr<T> model = StatModel::load<T>( filename_to_load );
    if( model.empty() )
        cout << "Could not read the classifier " << filename_to_load << endl;
    else
        cout << "The classifier " << filename_to_load << " is loaded.\n";
	
    return model;
}

static Ptr<TrainData>
prepare_train_data(const Mat& data, const Mat& responses, int ntrain_samples)
{
    Mat sample_idx = Mat::zeros( 1, data.rows, CV_8U );
    Mat train_samples = sample_idx.colRange(0, ntrain_samples);
    train_samples.setTo(Scalar::all(1));
	
    int nvars = data.cols;
    Mat var_type( nvars + 1, 1, CV_8U );
    var_type.setTo(Scalar::all(VAR_ORDERED));
    var_type.at<uchar>(nvars) = VAR_CATEGORICAL;
	
    return TrainData::create(data, ROW_SAMPLE, responses,
                             noArray(), sample_idx, noArray(), var_type);
}

inline TermCriteria TC(int iters, double eps)
{
    return TermCriteria(TermCriteria::MAX_ITER + (eps > 0 ? TermCriteria::EPS : 0), iters, eps);
}

static void test_and_save_classifier(const Ptr<StatModel>& model,
                                     const Mat& data, const Mat& responses,
                                     int ntrain_samples, int rdelta,
                                     const string& filename_to_save)
{
    int i, nsamples_all = data.rows;
    double train_hr = 0, test_hr = 0;
	
	if (my_rank == 0) {
		//save classifier first so other processors have access
		if( !filename_to_save.empty() )
		{
			model->save( filename_to_save );
		}
		int fin = 1;
		printf("RANK: %d", my_rank);

		MPI_Bcast(&fin, 1, MPI_INT, 0, MPI_COMM_WORLD);

		printf("RANK: %d", my_rank);
	}
	MPI_Barrier(MPI_COMM_WORLD);

	if (my_rank == 0) {
		// compute prediction error on train and test data
		int curr_task = 0;
		for(i = 0; i < num_procs; i++) {
			if (curr_task < nsamples_all) {
				MPI_Send(&curr_task, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
				curr_task++;
			}
		}
		int r;
		MPI_Status status;
		while (curr_task < nsamples_all) {
			MPI_Recv(&r, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
			MPI_Send(&curr_task, 1, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
			curr_task++;
		}
	}
	else {
		int i;
		MPI_Status status;
		MPI_Recv(&i, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
		
		printf("RANK: %d", my_rank);


		Mat sample = data.row(i);
		
		float r = model->predict( sample );
		r = std::abs(r + rdelta - responses.at<int>(i)) <= FLT_EPSILON ? 1.f : 0.f;
		
		if( i < ntrain_samples )
			train_hr += r;
		else
			test_hr += r;	
		
		MPI_Send(&i, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
	}
	
	MPI_Reduce(&train_hr, &train_hr, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&test_hr, &test_hr, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    test_hr /= nsamples_all - ntrain_samples;
    train_hr = ntrain_samples > 0 ? train_hr/ntrain_samples : 1.;
	
	if (my_rank == 0) {
		printf( "Recognition rate: train = %.1f%%, test = %.1f%%\n",
			   train_hr*100., test_hr*100. );
		
		if( !filename_to_save.empty() )
		{
			model->save( filename_to_save );
		}
	}
}


static bool
build_rtrees_classifier( const string& data_filename,
						const string& filename_to_save,
						const string& filename_to_load )
{
    Mat data;
    Mat responses;
    bool ok = read_num_class_data( data_filename, 16, &data, &responses );
    if( !ok )
        return ok;
	
    Ptr<RTrees> model;
	
    int nsamples_all = data.rows;
    int ntrain_samples = (int)(nsamples_all*0.8);
	
    // Create or load Random Trees classifier
    if( !filename_to_load.empty() )
    {
        model = load_classifier<RTrees>(filename_to_load);
        if( model.empty() )
            return false;
        ntrain_samples = 0;
    }
    else
    {
        // create classifier by using <data> and <responses>
        cout << "Training the classifier ...\n";
		//        Params( int maxDepth, int minSampleCount,
		//                   double regressionAccuracy, bool useSurrogates,
		//                   int maxCategories, const Mat& priors,
		//                   bool calcVarImportance, int nactiveVars,
		//                   TermCriteria termCrit );
        Ptr<TrainData> tdata = prepare_train_data(data, responses, ntrain_samples);
        model = RTrees::create();
        model->setMaxDepth(10);
        model->setMinSampleCount(10);
        model->setRegressionAccuracy(0);
        model->setUseSurrogates(false);
        model->setMaxCategories(15);
        model->setPriors(Mat());
        model->setCalculateVarImportance(true);
        model->setActiveVarCount(4);
        model->setTermCriteria(TC(100,0.01f));
        model->train(tdata);
        cout << endl;
    }
	
    test_and_save_classifier(model, data, responses, ntrain_samples, 0, filename_to_save);
    cout << "Number of trees: " << model->getRoots().size() << endl;
	
    // Print variable importance
    Mat var_importance = model->getVarImportance();
    if( !var_importance.empty() )
    {
        double rt_imp_sum = sum( var_importance )[0];
        printf("var#\timportance (in %%):\n");
        int i, n = (int)var_importance.total();
        for( i = 0; i < n; i++ )
            printf( "%-2d\t%-4.1f\n", i, 100.f*var_importance.at<float>(i)/rt_imp_sum);
    }
	
    return true;
}



int main( int argc, char *argv[] )
{
    string filename_to_save = "";
    string filename_to_load = "";
    string data_filename = "../opencv-3.0.0/samples/data/letter-recognition.data";
    int method = 0;
	int fin;
	
	MPI_Group orig_group, row_group, col_group;
	MPI_Comm row_comm, col_comm;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	
	// Start the timer
	double start_time;
	MPI_Barrier(MPI_COMM_WORLD);
	if (my_rank == 0) {  
		printf("RANK: %d", my_rank);

		start_time = MPI_Wtime();
	}
	
    int i;
    for( i = 1; i < argc; i++ )
    {
        if( strcmp(argv[i],"-data") == 0 ) // flag "-data letter_recognition.xml"
        {
            i++;
            data_filename = argv[i];
        }
        else if( strcmp(argv[i],"-save") == 0 ) // flag "-save filename.xml"
        {
            i++;
            filename_to_save = argv[i];
        }
        else if( strcmp(argv[i],"-load") == 0) // flag "-load filename.xml"
        {
            i++;
            filename_to_load = argv[i];
        }
        else
            break;
    }
	
	if(my_rank == 0) {
		printf("RANK: %d", my_rank);

		filename_to_save = "tree";
		fin = 1;
		if( i < argc ||
		   (method == 0 ?
			build_rtrees_classifier( data_filename, filename_to_save, filename_to_load ) :
			method == 1 ))
		{
			help();
		}
		printf("RANK: %d", my_rank);
		fprintf(stdout,"COMMUNICATION RUNTIME: %f - %f = %f\n", MPI_Wtime(), start_time, MPI_Wtime() - start_time);
		start_time = MPI_Wtime();
	}
	else {
		MPI_Bcast(&fin, 1, MPI_INT, 0, MPI_COMM_WORLD);
		filename_to_load = "tree";
		if( i < argc ||
		   (method == 0 ?
			build_rtrees_classifier( data_filename, filename_to_save, filename_to_load ) :
			method == 1 ))
		{
			help();
		}
		printf("RANK: %d", my_rank);
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		fprintf(stdout,"COMMUNICATION RUNTIME: %f - %f = %f\n", MPI_Wtime(), start_time, MPI_Wtime() - start_time);
	}
	// Clean-up
	MPI_Finalize();
	
    return 0;
}
