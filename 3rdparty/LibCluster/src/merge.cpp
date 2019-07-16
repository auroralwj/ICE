/*
   Implementation of "Highly efficient incremental estimation of Gaussian mixture models for online data stream clustering."

   Used to merge multiple mixture models

   Author: Ryan

 */

#include <omp.h>
#include <vector>
#include <numeric>
#include <stdexcept>
#include <iostream>
#include <Eigen/Dense>


#include "merge.h"
#include "libcluster.h"
#include "probutils.h"
#include "comutils.h"
#include "distributions.h"


#include <boost/math/distributions/fisher_f.hpp>
#include <boost/math/distributions/chi_squared.hpp>

using namespace std;
using namespace Eigen;
using namespace boost;
using namespace comutils;
using namespace probutils;
using namespace libcluster;
using namespace boost::math;
using namespace distributions;

using boost::math::chi_squared;
using boost::math::quantile;
using boost::math::complement;

//
// /*! \brief Merge similar mixture components as specified in the reference provided below.
//
//    REF ::
//    [1] Song, Mingzhou, and Hongbin Wang. "Highly efficient incremental estimation of Gaussian mixture models for online data stream clustering." Intelligent Computing: Theory and Applications III. Vol. 5803. International Society for Optics and Photonics, 2005.
//  */
//
//
// Author: Ryan Watson
//



bool merge::checkCov(Eigen::MatrixXd data, Eigen::MatrixXd qZ, mixtureComponents priorModel, GaussWish currModel, double currWeight, int currModelIdx, double alpha)
{
        Eigen::MatrixXd transObs, chol, chol_inv, tmp;

        chol = (priorModel.get<4>()).llt().matrixL();
        chol_inv = chol.inverse();

        int n = currModel.getN() + 1;
        int d = (currModel.getmean()).size() + 1;

        chi_squared dist((d*(d+1))/2);
        double ucv = quantile(complement(dist, alpha));
        double ucv2 = quantile(complement(dist, alpha/2.0));
        double lcv = quantile(dist, alpha);
        double lcv2 = quantile(dist, alpha/2.0);

        ArrayXi mapidx = partobs(data, (qZ.col(currModelIdx).array()>0.5), tmp);
        transObs.setZero(tmp.rows(),d);
        for (int k = 0; k < tmp.rows(); k++)
        {
                transObs.row(k) = chol_inv * tmp.row(k); // data.row(mapidx[k]);
        }

        auto transCov = probutils::cov(transObs);
        auto covDiff = transCov - Eigen::MatrixXd::Identity(d,d);
        auto covDiff_sq = covDiff * covDiff;
        double dd = static_cast<double>(d);
        double nn = static_cast<double>(n);
        double f = (1.0/dd) * ( (covDiff_sq).trace() );
        double s = (dd/nn) * pow( (1.0/dd)*(transCov.trace()),2 );
        double W = f - s + dd/nn;
        double t_stat = ((nn*W*dd)/2.0);
        if ( t_stat < ucv2 || t_stat > lcv2 ) { return true; }
        return false;
}

bool merge::checkMean(mixtureComponents priorModel, GaussWish currModel, double currWeight, double alpha)
{

        int n = currModel.getN() + 1;
        int d = (currModel.getmean()).size() + 1;
        fisher_f dist(d, n-d);

        double ucv = quantile(complement(dist, alpha));
        double ucv2 = quantile(complement(dist, alpha/2.0));
        double lcv = quantile(dist, alpha);
        double lcv2 = quantile(dist, alpha/2.0);


        Eigen::RowVectorXd meanDiff = currModel.getmean() - priorModel.get<3>();
        double t = (meanDiff * currModel.getcov() * meanDiff.transpose());
        double t_stat = (double)(n-d)/(double)(d*(n-1)) * pow(t,2.0);
        if (t_stat < ucv) {  return true; }
        return false;
}


vector<double> merge::getPriorWeights(vector<mixtureComponents> gmm)
{
        vector<double> weights;
        for(unsigned int i=0; i<gmm.size(); i++)
        {
                weights.push_back(gmm[i].get<2>());
        }
        return weights;
}

bool merge::checkComponent(Eigen::MatrixXd data, Eigen::MatrixXd qZ, mixtureComponents priorModel, GaussWish currModel, double currWeight, int currModelIdx, double alpha)
{
        if (checkCov(data, qZ, priorModel, currModel, currWeight, currModelIdx, alpha))
        {
                if (checkMean(priorModel, currModel, currWeight, alpha))
                        return true;
                else
                        return false;
        }
        return false;
}

bool merge::sortbyobs(merge::mixtureComponents a, merge::mixtureComponents b)
{
        return (a.get<1>() < b.get<1>());
}


vector<merge::mixtureComponents> merge::pruneMixtureModel(vector<merge::mixtureComponents> gmm, int truncLevel)
{
        if (gmm.size() < truncLevel)
                return gmm;


        merge::mixtureComponents currComponent;
        sort(gmm.begin(), gmm.end(), sortbyobs);
        while (gmm.size() > truncLevel)
        {
                currComponent = gmm[0];
                int n = currComponent.get<1>();
                for (unsigned int i=1; i<gmm.size(); i++)
                {
                        // update number of obs. for each component
                        gmm[i].get<0>() = gmm[i].get<0>() - n;
                        // update weight for each component
                        gmm[i].get<2>() = (double)gmm[i].get<1>() / (double)gmm[i].get<0>();
                }
                gmm.erase(gmm.begin());
        }


        return gmm;
}

vector<merge::mixtureComponents> merge::mergeMixtureModel(Eigen::MatrixXd data, Eigen::MatrixXd qZ, vector<merge::mixtureComponents> priorModel, vector<GaussWish> currModel, StickBreak currWeight, double alpha, int truncLevel)
{
        vector<merge::mixtureComponents> gmm;
        int dataCard = data.rows();

        auto pWeights =  getPriorWeights(priorModel);
        auto cWeights =  currWeight.Elogweight().exp().transpose();
        int cc = 0;
        vector<int> pModelMatches;
        vector<int> cModelMatches;

        if ( priorModel.size() == 0)
        {
                for (vector<GaussWish>::iterator j = currModel.begin(); j < currModel.end(); ++j)
                {
                        // num obs.
                        int n = j->getN();
                        // cluster weights
                        double w = cWeights[cc];
                        // cluster mean
                        auto m = j->getmean();
                        // cluster cov
                        auto c = j->getcov();

                        gmm.push_back(boost::make_tuple(dataCard, n, w, m, c));
                        cc+=1;
                }
                return gmm;
        }

        for (unsigned int i=0; i<priorModel.size(); i++)
        {
                for (vector<GaussWish>::iterator j = currModel.begin(); j < currModel.end(); ++j)
                {
                        if( checkComponent(data, qZ, priorModel[i], *j, cWeights[cc], cc, alpha)
                            && !(std::find(pModelMatches.begin(), pModelMatches.end(), i) != pModelMatches.end())
                            && !(std::find(cModelMatches.begin(), cModelMatches.end(), cc) != cModelMatches.end()) )
                        {
                                // merge components
                                mixtureComponents pModel = priorModel[i];
                                // update mean and cov based on eq.

                                int NP = pModel.get<0>();
                                int n = pModel.get<1>() +j->getN();
                                double WP = pModel.get<2>();
                                Eigen::RowVectorXd MP = pModel.get<3>();
                                Eigen::MatrixXd CM = pModel.get<4>();
                                double NPWP = NP*WP;

                                Eigen::RowVectorXd MC = j->getmean();
                                Eigen::MatrixXd CC = j->getcov();
                                int NC = j->getN();

                                auto denom = NPWP + NC;

                                /// update mean --> REF:: [1] Eq. 6
                                Eigen::RowVectorXd n_mean = (NPWP*MP + NC*MC )*(1/denom);

                                Eigen::MatrixXd A = (NPWP*CM + NC*CC)*(1/denom);

                                Eigen::MatrixXd cov_p = MP.transpose()*MP;
                                Eigen::MatrixXd cov_c = MC.transpose()*MC;

                                Eigen::MatrixXd B = ( NPWP*cov_p + NC*cov_c )*(1/denom);

                                Eigen::MatrixXd C = n_mean.transpose() * n_mean;

                                /// update cov --> REF:: [1] Eq. 7
                                Eigen::MatrixXd n_cov = A + B - C;

                                double w =  (NPWP + NC) /(double) (NP + dataCard);

                                gmm.push_back(boost::make_tuple(NP+dataCard, n, w, n_mean, n_cov));
                                pModelMatches.push_back(i);
                                cModelMatches.push_back(cc);
                        }
                        cc+=1;
                }
                cc=0;
        }

        int idx = 0;
        for (vector<GaussWish>::iterator j = currModel.begin(); j < currModel.end(); ++j)
        {
                if (!(std::find(cModelMatches.begin(), cModelMatches.end(), idx) != cModelMatches.end()))
                {
                        Eigen::RowVectorXd MC = j->getmean();
                        Eigen::MatrixXd CC = j->getcov();
                        int NC = j->getN();
                        int n = priorModel[0].get<0>() + dataCard;


                        double w = NC/(double)n;

                        gmm.push_back(boost::make_tuple(n, NC, w, MC, CC));
                }

                idx+=1;
        }

        idx = 0;
        for (unsigned int i=0; i<priorModel.size(); i++)
        {
                mixtureComponents pModel;
                pModel = priorModel[i];
                if (!(std::find(pModelMatches.begin(), pModelMatches.end(), idx) != pModelMatches.end()))
                {
                        int NP = pModel.get<1>();
                        int n = pModel.get<0>() + dataCard;
                        double w = NP/(double)n;
                        Eigen::RowVectorXd MP = pModel.get<3>();
                        Eigen::MatrixXd CM = pModel.get<4>();

                        gmm.push_back(boost::make_tuple(n, NP, w, MP, CM));
                }

                idx+=1;
        }

        gmm = pruneMixtureModel(gmm, truncLevel);
        return gmm;
}


merge::observationModel merge::getMixtureComponent(vector<merge::mixtureComponents> gmm, Eigen::VectorXd observation)
{

        int idx = 0;
        double maxProb = 0, modelProb;
        Eigen::RowVectorXd mean;
        Eigen::MatrixXd cov;
        double doublePi = M_PI * 2.0;

        for (int i=0; i<gmm.size(); i++)
        {

                mean = gmm[i].get<3>();
                cov = gmm[i].get<4>();
                double err = (-1/2)*(observation-mean).transpose()*cov.inverse()*(observation-mean);

                modelProb = (1/(sqrt(doublePi*cov.determinant()))) * (exp(err));
                if (modelProb > maxProb)
                {
                        maxProb = modelProb;
                        idx = i;
                }
        }

        merge::mixtureComponents gmmComp = gmm[idx];
        merge::observationModel obsModel = boost::make_tuple(gmmComp, maxProb);

        return obsModel;
}
