//
// Distributed under the ITensor Library License, Version 1.2
//    (See accompanying LICENSE file.)
//
#ifndef __ITENSOR_ITCPLX_H
#define __ITENSOR_ITCPLX_H

#include "itensor/itdata/itreal.h"

namespace itensor {


//
// Optimization TODO: 
//  replace std::vector storage with
//  storage type only holding data ptr
//  and size
//

class ITCplx
    {
    public:
    using storage_type = std::vector<Real>;
    using size_type = storage_type::size_type;
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    //
    // Data members
    //

    storage_type store;

    //
    // Constructors
    //

    ITCplx() { }

    ITCplx(size_t size) : store(2*size,0) { }

    //Set all elements equal to (val,0)
    ITCplx(size_t size, Real val) 
        : 
        store(2*size,0) 
        { 
        std::fill(store.begin(),store.begin()+csize(),val);
        }
    //Set all elements equal to (val.real(),val.imag())
    ITCplx(size_t size, const Cplx& val) 
        : 
        store(2*size) 
        { 
        fill(val);
        }

    explicit
    ITCplx(const ITReal& d);

    template<typename InputIterator>
    ITCplx(InputIterator b, InputIterator e) : store(b,e) { }

    //
    // Accessors
    //

    Cplx
    get(size_type i) const
        {
        return Cplx(store[i],store[csize()+i]);
        }

    void
    set(size_type i, const Cplx& z) 
        {
        store[i] = z.real();
        store[csize()+i] = z.imag();
        }

    ITCplx&
    operator*=(const Cplx& z);

    void
    fill(const Cplx& z);

    size_type
    csize() const { return store.size()/2; }

    Real*
    rstart() { return store.data(); }
    const Real*
    rstart() const { return store.data(); }
    const Real*
    rend() const { return store.data()+csize(); }

    Real*
    istart() { return store.data()+csize(); }
    const Real*
    istart() const { return store.data()+csize(); }
    const Real*
    iend() const { return store.data()+store.size(); }

    //
    // std container like methods
    //

    size_type
    size() const { return store.size(); }
    bool
    empty() const { return store.empty(); }

    Real*
    data() { return store.data(); }
    const Real*
    data() const { return store.data(); }
    
    const_iterator
    cbegin() const { return store.cbegin(); }
    const_iterator
    cend() const { return store.cend(); }
    const_iterator
    begin() const { return store.begin(); }
    const_iterator
    end() const { return store.end(); }
    iterator
    begin() { return store.begin(); }
    iterator
    end() { return store.end(); }

    };

void inline
read(std::istream& s, ITCplx& dat)
    {
    read(s,dat.store);
    }

void inline
write(std::ostream& s, const ITCplx& dat)
    {
    write(s,dat.store);
    }

template<typename F>
void
doTask(ApplyIT<F>& A, ITCplx& d)
    { 
    for(auto j = 0ul; j < d.csize(); ++j)
        {
        auto res = detail::call<Cplx>(A.f,d.get(j));
        d.set(j,res);
        }
    }

template<typename F>
void
doTask(VisitIT<F>& V, const ITCplx& d)
    { 
    for(auto j = 0ul; j < d.csize(); ++j)
        {
        detail::call<void>(V.f,V.scale_fac * d.get(j));
        }
    }

template<typename F>
void
doTask(GenerateIT<F,Cplx>& G, ITCplx& d)
    { 
    for(auto j = 0ul; j < d.csize(); ++j)
        d.set(j,G.f());
    }

template<typename F>
void
doTask(GenerateIT<F,Real>& G, const ITCplx& d, ManageStore& m)
    { 
    auto* nd = m.makeNewData<ITReal>(d.csize());
    std::generate(nd->begin(),nd->end(),G.f);
    }

//
// This doTask(...ITReal...) method intentionally placed here
// since it requires definition of class ITCplx
//
template<typename F>
void
doTask(GenerateIT<F,Cplx>& G, const ITReal& d, ManageStore& m)
    { 
    auto* nd = m.makeNewData<ITCplx>(d.size());
    for(auto j = 0ul; j < nd->csize(); ++j)
        nd->set(j,G.f());
    }


Cplx
doTask(const GetElt<Index>& g, const ITCplx& d);

void
doTask(const SetElt<Real,Index>& s, ITCplx& d);

void
doTask(const SetElt<Cplx,Index>& s, ITCplx& d);

void
doTask(Contract<Index>& C,
       const ITCplx& a1,
       const ITCplx& a2,
       ManageStore& m);

void
doTask(Contract<Index>& C,
       const ITReal& a1,
       const ITCplx& a2,
       ManageStore& m);

void
doTask(Contract<Index>& C,
       const ITCplx& a1,
       const ITReal& a2,
       ManageStore& m);

void
doTask(const FillReal& f, const ITCplx& d, ManageStore& m);

void
doTask(const FillCplx& f, ITCplx& d);

void
doTask(const MultCplx& M, ITCplx& d); 

void
doTask(const MultReal& m, ITCplx& d);

Real
doTask(NormNoScale, const ITCplx& d);

void
doTask(Conj, ITCplx& d); 

void
doTask(TakeReal,ITCplx& d, ManageStore& m);

void
doTask(TakeImag,const ITCplx& d, ManageStore& m); 

void
doTask(PrintIT<Index>& P, const ITCplx& d);

bool
doTask(CheckComplex,const ITCplx& d);

Cplx
doTask(SumEls<Index>, const ITCplx& d);

void
doTask(Write& W, const ITCplx& d);

} //namespace itensor

#endif
