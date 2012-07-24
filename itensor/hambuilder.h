//
// Distributed under the ITensor Library License, Version 1.0.
//    (See accompanying LICENSE file.)
//
#ifndef __ITENSOR_HAMBUILDER_H
#define __ITENSOR_HAMBUILDER_H
#include "mpo.h"

//
//
// HamBuilder
//
// Class for creating product-operator MPOs,
// usually to be combined into a more complex
// MPO such as a Hamiltonian.
//

class HamBuilder
    {
    public:

    HamBuilder(const Model& mod);

    int
    NN() const { return N_; }

    //Sets res to an identity MPO
    template <class Tensor>
    void
    getMPO(MPOt<Tensor>& res, Real fac=1) const;

    //Sets the j1'th operator of res to op1
    //and the rest to the identity
    template <class Tensor>
    void
    getMPO(int j1, const Tensor& op1, 
           MPOt<Tensor>& res, Real fac=1) const;

    //Sets the j1'th operator of res to op1,
    //the j2'th operator of res to op2,
    //and the rest to the identity
    template <class Tensor>
    void
    getMPO(int j1, const Tensor& op1,
           int j2, const Tensor& op2,
           MPOt<Tensor>& res, Real fac=1) const;

    // etc.

    template <class Tensor>
    void
    getMPO(int j1, const Tensor& op1,
           int j2, const Tensor& op2,
           int j3, const Tensor& op3,
           MPOt<Tensor>& res, Real fac=1) const;

    template <class Tensor>
    void
    getMPO(int j1, const Tensor& op1,
           int j2, const Tensor& op2,
           int j3, const Tensor& op3,
           int j4, const Tensor& op4,
           MPOt<Tensor>& res, Real fac=1) const;

    private:

    /////////////////
    //
    // Data Members

    const Model& mod_;
    const int N_;

    //
    /////////////////

    void
    putLinks(MPOt<ITensor>& res) const;
    void
    putLinks(MPOt<IQTensor>& res) const;

    template <class Tensor>
    void
    initialize(MPOt<Tensor>& res) const;

    static int
    hamNumber()
        {
        static int num_ = 0;
        ++num_;
        return num_;
        }


    };

inline HamBuilder::
HamBuilder(const Model& mod)
    :
    mod_(mod),
    N_(mod.NN())
    { }

template <class Tensor>
void HamBuilder::
getMPO(MPOt<Tensor>& res, Real fac) const
    {
    initialize(res);
    putLinks(res);
    res.AAnc(1) *= fac;
    }

template <class Tensor>
void HamBuilder::
getMPO(int j1, const Tensor& op1, 
       MPOt<Tensor>& res, Real fac) const
    {
    initialize(res);
#ifdef DEBUG
    if(!op1.hasindex(mod_.si(j1)))
        {
        Print(j1);
        PrintIndices(op1);
        Error("Tensor does not have correct Site index");
        }
#endif
    res.AAnc(j1) = op1;
    res.AAnc(j1) *= fac;
    putLinks(res);
    }

template <class Tensor>
void HamBuilder::
getMPO(int j1, const Tensor& op1,
       int j2, const Tensor& op2,
       MPOt<Tensor>& res, Real fac) const
    {
    initialize(res);
#ifdef DEBUG
    if(!op1.hasindex(mod_.si(j1)))
        {
        Print(j1);
        PrintIndices(op1);
        Error("Tensor does not have correct Site index");
        }
    if(!op2.hasindex(mod_.si(j2)))
        {
        Print(j2);
        PrintIndices(op2);
        Error("Tensor does not have correct Site index");
        }
#endif
    res.AAnc(j1) = op1;
    res.AAnc(j2) = op2;
    res.AAnc(j1) *= fac;
    putLinks(res);
    }

template <class Tensor>
void HamBuilder::
getMPO(int j1, const Tensor& op1,
       int j2, const Tensor& op2,
       int j3, const Tensor& op3,
       MPOt<Tensor>& res, Real fac) const
    {
    initialize(res);
#ifdef DEBUG
    if(!op1.hasindex(mod_.si(j1)))
        {
        Print(j1);
        PrintIndices(op1);
        Error("Tensor does not have correct Site index");
        }
    if(!op2.hasindex(mod_.si(j2)))
        {
        Print(j2);
        PrintIndices(op2);
        Error("Tensor does not have correct Site index");
        }
    if(!op3.hasindex(mod_.si(j3)))
        {
        Print(j3);
        PrintIndices(op3);
        Error("Tensor does not have correct Site index");
        }
#endif
    res.AAnc(j1) = op1;
    res.AAnc(j2) = op2;
    res.AAnc(j3) = op3;
    res.AAnc(j1) *= fac;
    putLinks(res);
    }

template <class Tensor>
void HamBuilder::
getMPO(int j1, const Tensor& op1,
       int j2, const Tensor& op2,
       int j3, const Tensor& op3,
       int j4, const Tensor& op4,
       MPOt<Tensor>& res, Real fac) const
    {
    initialize(res);
#ifdef DEBUG
    if(!op1.hasindex(mod_.si(j1)))
        {
        Print(j1);
        PrintIndices(op1);
        Error("Tensor does not have correct Site index");
        }
    if(!op2.hasindex(mod_.si(j2)))
        {
        Print(j2);
        PrintIndices(op2);
        Error("Tensor does not have correct Site index");
        }
    if(!op3.hasindex(mod_.si(j3)))
        {
        Print(j3);
        PrintIndices(op3);
        Error("Tensor does not have correct Site index");
        }
    if(!op4.hasindex(mod_.si(j4)))
        {
        Print(j4);
        PrintIndices(op4);
        Error("Tensor does not have correct Site index");
        }
#endif
    res.AAnc(j1) = op1;
    res.AAnc(j2) = op2;
    res.AAnc(j3) = op3;
    res.AAnc(j4) = op4;
    res.AAnc(j1) *= fac;
    putLinks(res);
    }


template <class Tensor>
void HamBuilder::
initialize(MPOt<Tensor>& res) const
    {
    res = MPOt<Tensor>(mod_);
    for(int j = 1; j <= N_; ++j)
        res.AAnc(j) = mod_.id(j);
    }


void HamBuilder::
putLinks(MPOt<ITensor>& res) const
    {
    int ver = hamNumber();
    std::vector<Index> links(N_);
    for(int i = 1; i < N_; ++i)
        {
        boost::format nm = boost::format("h%d-%d") % ver % i;
        links.at(i) = Index(nm.str());
        }
    res.AAnc(1) *= links.at(1)(1);
    for(int i = 1; i < N_; ++i)
        {
        res.AAnc(i) *= links.at(i-1)(1);
        res.AAnc(i) *= links.at(i)(1);
        }
    res.AAnc(N_) *= links.at(N_-1)(1);
    }

void HamBuilder::
putLinks(MPOt<IQTensor>& res) const
    {
    QN q;

    int ver = hamNumber();
    std::vector<IQIndex> links(N_);
    for(int i = 1; i < N_; ++i)
        {
        boost::format nm = boost::format("h%d-%d") % ver % i,
                      Nm = boost::format("H%d-%d") % ver % i;
        q += res.AA(i).div();
        links.at(i) = IQIndex(Nm.str(),
                             Index(nm.str()),q);
        }

    res.AAnc(1) *= links.at(1)(1);
    for(int i = 2; i < N_; ++i)
        {
        res.AAnc(i) *= conj(links.at(i-1)(1));
        res.AAnc(i) *= links.at(i)(1);
        }
    res.AAnc(N_) *= conj(links.at(N_-1)(1));
    }

#endif