//TODO: replace unordered_map with a simpler container (small_map? or jump directly to location?)
#include <unordered_map>
#include <future>

#include "itensor/util/multalloc.h"
#include "itensor/util/cputime.h"
#include "itensor/matrix/mat.h"
#include "itensor/tensor/contract.h"
#include "itensor/detail/algs.h"
#include "itensor/detail/gcounter.h"
#include "itensor/indexset.h"

using std::vector;

namespace itensor {

template<typename T>
void
printv(const vector<T>& t)
    {
    print("{ ");
    for(const auto& i : t) print(i," ");
    println("}");
    }
template<typename T,typename F>
void
printv(const vector<T>& t,
       const F& f)
    {
    print("{ ");
    for(const auto& i : t) 
        {
        f(i);
        print(" ");
        }
    println("}");
    }
template<typename T, size_t size>
void
printv(const std::array<T,size>& t)
    {
    print("{ ");
    for(const auto& i : t) print(i," ");
    println("}");
    }
template<typename T>
void
printv(const autovector<T>& t)
    {
    print("{ ");
    for(auto i = t.mini(); i <= t.maxi(); ++i)
        {
        print(t[i]," ");
        }
    println("}");
    }
#define PRI(a) print(#a,": "); printv(a);
#define PRIL(a,l) print(#a,": "); printv(a,l);

//template<typename T>
//long 
//find_index(vector<T> const& v, 
//           T const& t)
//    {
//    using size_type = typename vector<T>::size_type;
//    for(size_type i = 0; i < v.size(); ++i)
//        if(v[i] == t) return i;
//    return -1;
//    }
//
//template<typename T, size_t MaxSize>
//long 
//find_index(const VarArray<T,MaxSize>& v, 
//           const T& t)
//    {
//    using size_type = typename VarArray<T,MaxSize>::size_type;
//    for(size_type i = 0; i < v.size(); ++i)
//        if(v[i] == t) return i;
//    return -1;
//    }
//
//template<typename T, size_t MaxSize>
//long 
//find_index(const InfArray<T,MaxSize>& v, 
//           const T& t)
//    {
//    using size_type = typename InfArray<T,MaxSize>::size_type;
//    for(size_type i = 0; i < v.size(); ++i)
//        if(v[i] == t) return i;
//    return -1;
//    }

//
// small_map has an interface similar to std::map
// but can only contain N elements and uses linear search
//
template<typename A, typename B, int N = 30>
class small_map
    {
    public:

    std::array<std::pair<A,B>,N> d;
    int nd{0};

    B& 
    operator[](const A& a)
        {
        for(int i = 0; i < nd; ++i)
            {
            if(d[i].first == a) return d[i].second;
            }
        if(++nd >= N) error("couldnt use small_map, nd too big");
        d[nd-1] = std::make_pair(a,B{});
        return d[nd-1].second;
        }
    };

// struct analyzing index pattern for C = A * B
struct CProps
    {
    Label ai, 
          bi, 
          ci;
    int nactiveA = 0, 
        nactiveB = 0, 
        nactiveC = 0;
    private:
    Label AtoB_, 
          AtoC_, 
          BtoC_;
    bool permuteA_ = false,
         permuteB_ = false,
         permuteC_ = false;
    public:
    using Dimension = unsigned long long;
    Dimension dleft = 1,
              dmid = 1,
              dright = 1;
    int ncont = 0,
        Acstart,
        Bcstart,
        Austart,
        Bustart;
    Permutation PA,
                PB,
                PC;
    bool ctrans = false;
    Range newArange,
          newBrange,
          newCrange;
    
    CProps(const Label& ai_, 
           const Label& bi_, 
           const Label& ci_) 
        : 
        ai(ai_),bi(bi_),ci(ci_),
        Acstart(ai_.size()),
        Bcstart(bi_.size()),
        Austart(ai_.size()),
        Bustart(bi_.size())
        { }

    CProps(const CProps&) = delete;

    CProps& operator=(const CProps&) = delete;

    bool
    contractedA(int i) const { return AtoC_[i] < 0; }
    bool
    contractedB(int i) const { return BtoC_[i] < 0; }
    int
    AtoB(int i) const { return AtoB_[i]; }
    int
    AtoC(int i) const { return AtoC_[i]; }
    int
    BtoC(int i) const { return BtoC_[i]; }
    bool
    permuteA() const { return permuteA_; }
    bool
    permuteB() const { return permuteB_; }
    bool
    permuteC() const { return permuteC_; }
    bool
    Atrans() const { return contractedA(0); }
    bool
    Btrans() const { return !contractedB(0); }
    bool
    Ctrans() const { return ctrans; }

    private:
    bool
    checkACsameord() const
        {
        if(size_t(Austart) >= ai.size()) return true;
        auto aCind = AtoC(Austart);
        using size_type = decltype(ai.size());
        for(size_type i = 0; i < ai.size(); ++i) 
            if(!contractedA(i))
                {
                if(AtoC(i) != aCind) return false;
                ++aCind;
                }
        return true;
        }

    bool
    checkBCsameord() const
        {
        if(size_t(Bustart) >= bi.size()) return true;
        auto bCind = BtoC(Bustart);
        using size_type = decltype(bi.size());
        for(size_type i = 0; i < bi.size(); ++i) 
            if(!contractedB(i))
                {
                if(BtoC(i) != bCind) return false;
                ++bCind;
                }
        return true;
        }
    public:

    template<typename RangeT>
    void
    compute(RTenRefc<RangeT> A,
            RTenRefc<RangeT> B,
            RTenRefc<RangeT> C)
        {
        // Optimizations TODO
        //
        // o Add "automatic C" mode where index order of C can be
        //   unspecified, and will be chosen so as not to require
        //   permuting C at the end.
        //
        // o If trailing extent(j)==1 dimensions at end of A, B, or C indices
        //   (as often the case for ITensors with m==1 indices),
        //   have CProps resize ai, bi, and ci accordingly to avoid
        //   looping over these.
        //

        computePerms();
        
        //Use PC.size() as a check to see if we've already run this
        if(PC.size() != 0) return;

        int ra = ai.size(),
            rb = bi.size(),
            rc = ci.size();

        PC = Permutation(rc);

        dleft = 1;
        dmid = 1;
        dright = 1;
        int c = 0;
        for(int i = 0; i < ra; ++i)
            if(!contractedA(i))
                {
                dleft *= A.extent(i);
                PC.setFromTo(c,AtoC(i));
                ++c;
                }
            else
                {
                dmid *= A.extent(i);
                }
        for(int j = 0; j < rb; ++j)
            if(!contractedB(j)) 
                {
                dright *= B.extent(j);
                PC.setFromTo(c,BtoC(j));
                ++c;
                }

        if(!isTrivial(PC)) 
            {
            permuteC_ = true;
            if(checkBCsameord() && checkACsameord())
                {
                //Can avoid permuting C by 
                //computing Bt*At = Ct
                ctrans = true;
                permuteC_ = false;
                }
            }

        //Check if A can be treated as a matrix without permuting
        permuteA_ = false;
        if(!(contractedA(0) || contractedA(ra-1)))
            {
            //If contracted indices are not all at front or back, 
            //will have to permute A 
            permuteA_ = true;
            }
        else
            {
            //Contracted ind start at front or back, check if contiguous
            for(int i = 0; i < ncont; ++i)
                if(!contractedA(Acstart+i)) 
                    {
                    //Contracted indices not contiguous, must permute
                    permuteA_ = true;
                    break;
                    }
            }

        // Check if B is matrix-like
        permuteB_ = false;
        if(!(contractedB(0) || contractedB(rb-1)))
            {
            //If contracted indices are not all at front or back, 
            //will have to permute B
            permuteB_ = true;
            }
        else
            {
            for(int i = 0; i < ncont; ++i)
                if(!contractedB(Bcstart+i))
                    {
                    //Contracted inds not contiguous, permute
                    permuteB_ = true;
                    break;
                    }
            }

        if(!permuteA_ && !permuteB_)
            {
            //Check if contracted inds. in same order
            for(int i = 0; i < ncont; ++i)
                if(AtoB(Acstart+i) != (Bcstart+i)) 
                    {
                    //If not in same order, 
                    //must permute one of A or B
                    //so permute the smaller one
                    if(dleft < dright) permuteA_ = true;
                    else               permuteB_ = true;
                    break;
                    }
            }


        if(permuteC_ && !(permuteA_ && permuteB_))
            {
            auto PCost = [](Real d) { return d*d; };
            //Could avoid permuting C if
            //permute both A and B, worth it?
            auto pCcost = PCost(dleft*dright);
            Real extra_pABcost = 0;
            if(!permuteA_) extra_pABcost += PCost(dleft*dmid);
            if(!permuteB_) extra_pABcost += PCost(dmid*dright);
            if(extra_pABcost < pCcost)
                {
                //printfln("dleft=%d, dmid=%d, dright=%d",dleft,dmid,dright);
                //if(Global::debug1()) printfln("Permuting %s %s (%d) instead of permuting C (%d)",permuteA_?"":"A",permuteB_?"":"B",extra_pABcost,pCcost);
                permuteA_ = true;
                permuteB_ = true;
                permuteC_ = false;
                }
            }

        //if(Global::debug1()) printfln("At line %d: permute A,B,C = %s,%s,%s",__LINE__,permuteA_,permuteB_,permuteC_);

        if(permuteA_)
            {
            PA = Permutation(ra);
            //Permute contracted indices to the front,
            //in the same order as on B
            int newi = 0;
            auto bind = Bcstart;
            for(int i = 0; i < ncont; ++i)
                {
                while(!contractedB(bind)) ++bind;
                auto j = find_index(ai,bi[bind]);
                PA.setFromTo(j,newi++);
                ++bind;
                }
            //Reset p.AtoC:
            std::fill(AtoC_.begin(),AtoC_.end(),-1);
            //Permute uncontracted indices to
            //appear in same order as on C
            for(int k = 0; k < rc; ++k)
                {
                auto j = find_index(ai,ci[k]);
                if(j != -1)
                    {
                    AtoC_[newi] = k;
                    PA.setFromTo(j,newi);
                    ++newi;
                    }
                if(newi == ra) break;
                }
            //Compute newArange
            //Also update Austart,Acstart
            newArange.resize(ra);
            Acstart = ra;
            Austart = ra;
            for(int i = 0; i < ra; ++i)
                {
                newArange[PA.dest(i)].ext = A.extent(i);
                if(contractedA(i))
                    Acstart = std::min(i,Acstart);
                else
                    Austart = std::min(i,Austart);
                }
            newArange.computeStrides();
            }

        if(permuteB_)
            {
            PB = Permutation(rb);
            int newi = 0;
            if(permuteA_)
                {
                //A's contracted indices already set to
                //be in same order as B above, so just
                //permute contracted indices to the front
                //keeping relative order
                for(int i = Bcstart; newi < ncont; ++newi)
                    {
                    while(!contractedB(i)) ++i;
                    PB.setFromTo(i++,newi);
                    }
                }
            else
                {
                //Permute contracted indices to the
                //front and in same order as on A
                auto aind = Acstart;
                for(int i = 0; i < ncont; ++i)
                    {
                    while(!contractedA(aind)) ++aind;
                    int j = find_index(bi,ai[aind]);
                    PB.setFromTo(j,newi++);
                    ++aind;
                    }
                }
            //Reset p.BtoC:
            std::fill(BtoC_.begin(),BtoC_.end(),-1);
            //Permute uncontracted indices to
            //appear in same order as on C
            for(int k = 0; k < rc; ++k)
                {
                auto j = find_index(bi,ci[k]);
                if(j != -1)
                    {
                    BtoC_[newi] = k;
                    PB.setFromTo(j,newi);
                    ++newi;
                    }
                if(newi == rb) break;
                }
            newBrange.resize(rb);
            Bcstart = rb;
            Bustart = rb;
            for(int i = 0; i < rb; ++i)
                {
                newBrange[PB.dest(i)].ext = B.extent(i);
                if(contractedB(i))
                    Bcstart = std::min(i,Bcstart);
                else
                    Bustart = std::min(i,Bustart);
                }
            newBrange.computeStrides();
            }

        if(permuteA_ || permuteB_)
            {
            //Recompute PC
            int c = 0;
            for(int i = 0; i < ra; ++i)
                if(!contractedA(i))
                    {
                    PC.setFromTo(c,AtoC_[i]);
                    ++c;
                    }
            for(int j = 0; j < rb; ++j)
                if(!contractedB(j)) 
                    {
                    PC.setFromTo(c,BtoC_[j]);
                    ++c;
                    }
            ctrans = false;
            if(isTrivial(PC))
                {
                permuteC_ = false;
                }
            else
                {
                permuteC_ = true;
                //Here we already know since pc_triv = false that
                //at best indices from B precede those from A (on result C)
                //so if both sets remain in same order on C 
                //just need to transpose C, not permute it
                if(checkBCsameord() && checkACsameord()) 
                    {
                    ctrans = true;
                    permuteC_ = false;
                    }
                }
            }

        //PRI(AtoC_)
        //PRI(BtoC_)
        //Print(PC);

        if(permuteC_)
            {
            newCrange.resize(rc);
            int c = 0;

            if(!permuteA_)
                {
                for(int i = 0; i < ra; ++i)
                    if(!contractedA(i))
                        newCrange[c++].ext = A.extent(i);
                }
            else
                {
                for(int i = 0; i < ra; ++i)
                    if(!contractedA(i))
                        newCrange[c++].ext = newArange.extent(i);
                }
            if(!permuteB_)
                {
                for(int j = 0; j < rb; ++j)
                    if(!contractedB(j)) 
                        newCrange[c++].ext = B.extent(j);
                }
            else
                {
                for(int j = 0; j < rb; ++j)
                    if(!contractedB(j)) 
                        newCrange[c++].ext = newBrange.extent(j);
                }
            newCrange.computeStrides();
            }
        }

    void 
    computePerms()
        {
        //Use !AtoB.empty() as a check to see if we've already run this
        if(!AtoB_.empty()) return;

        int na = ai.size(), 
            nb = bi.size(), 
            nc = ci.size();

        AtoB_ = Label(na,-1);
        AtoC_ = Label(na,-1);
        BtoC_ = Label(nb,-1);
        for(int i = 0; i < na; ++i)
            {
            for(int j = 0; j < nb; ++j)
                if(ai[i] == bi[j]) 
                    {
                    ++ncont;
                    if(i < Acstart) Acstart = i;
                    if(j < Bcstart) Bcstart = j;
                    AtoB_[i] = j;
                    break;
                    }
            }

        for(int i = 0; i < na; ++i)
            {
            for(int k = 0; k < nc; ++k)
                if(ai[i] == ci[k]) 
                    {
                    if(i < Austart) Austart = i;
                    AtoC_[i] = k;
                    break;
                    }
            }

        for(int j = 0; j < nb; ++j)
            {
            for(int k = 0; k < nc; ++k)
                if(bi[j] == ci[k]) 
                    {
                    if(j < Bustart) Bustart = j;
                    BtoC_[j] = k;
                    break;
                    }
            }
        //PRI(AtoB_)
        //PRI(AtoC_)
        //PRI(BtoC_)
        }

    void
    computeNactive()
        {
        // Out of A, B and C  (C = A*B), each index appears in two tensors.
        // An active index appears as one of the first two indices in each of the two 
        // tensor in which it appears.  More specifically:
        // the first index of a tensor is active if its pair is also a first index, or if its
        // pair is a second index and that tensor's first index is active.

        // indval is the number of times an index appears among the first two of each tensor
        // Good indices have indval == 2, bad ones have indval == 1

        if(ai.size() < 2 || bi.size() < 2 || ci.size() < 2)
            {
            nactiveA = nactiveB = nactiveC = 0;
            return;
            }

        small_map<int,int> indval;
        for(int i = 0; i <= 1; ++i)
            {
            ++indval[ai[i]]; 
            ++indval[bi[i]]; 
            ++indval[ci[i]]; 
            }

        for(int elim = 1; elim <= 3; ++elim) // bad guys at position 0 kill the index at 1
            {
            if(indval[ai[0]] == 1) indval[ai[1]] = 1;
            if(indval[bi[0]] == 1) indval[bi[1]] = 1;
            if(indval[ci[0]] == 1) indval[ci[1]] = 1;
            }
        nactiveA = (indval[ai[0]] == 1 ? 0 : (indval[ai[1]] == 1 ? 1 : 2));
        nactiveB = (indval[bi[0]] == 1 ? 0 : (indval[bi[1]] == 1 ? 1 : 2));
        nactiveC = (indval[ci[0]] == 1 ? 0 : (indval[ci[1]] == 1 ? 1 : 2));
        }
    };


struct ABoffC
    {
    MatRefc mA, 
            mB;
    MatRef  mC;
    int offC;

    ABoffC(MatRefc& mA_, 
           MatRefc& mB_, 
           MatRef& mC_, 
           int offC_)
        : 
        mA(mA_), 
        mB(mB_), 
        mC(mC_), 
        offC(offC_) 
        { }

    void
    execute() const { multAdd(mA,mB,mC); }
    };

class CABqueue
    {
    std::unordered_map<int,vector<ABoffC>> subtask;
    public:

    CABqueue() { }

    void 
    addtask(MatRefc& mA, 
            MatRefc& mB, 
            MatRef& mC, 
            int offC)
        {
        subtask[offC].emplace_back(mA,mB,mC,offC);
        }

    void 
    run(int numthread)
        {

        //////////
        ////Analyze tasks:
        //println("number of distinct offCs is ",subtask.size());
        //int ttasks = 0;
        //for(auto& st : subtask) ttasks += st.second.size();
        //println("number of distinct tasks is ",ttasks);
        //size_t maxj = 0;
        //for(const auto& st : subtask)
        //    maxj = std::max(st.second.size(),maxj);
        //println("max subtask size is ",maxj);
        //////////

        //Loop over threads in a round-robin fashion.
        //Assign all tasks with the same memory 
        //destination (offC) to the same thread. 
        vector<vector<ABoffC>> threadtask(numthread);
        int ss = 0;
        for(auto& t : subtask)
            {
            auto& st_tasks = t.second;
            auto& tt = threadtask[ss];
            std::move(st_tasks.begin(),st_tasks.end(),std::inserter(tt,tt.end()));
            ss = (ss+1)%numthread;
            }

        //"Package" thread tasks into std::future objects
        //which begin running once they are created
        vector<std::future<void>> futs(numthread);
        assert(threadtask.size()==futs.size());
        for(size_t i = 0; i < futs.size(); ++i)
            {
            auto& tt = threadtask[i];
            //printfln("task size for thread %d is %d",i,tt.size());
            futs[i] = std::async(std::launch::async,
                      [&tt]()
                          { 
                          for(const auto& task : tt)
                            task.execute();
                          }
                      );
            }
        //Wait for futures to complete
        for(auto& ft : futs) 
            {
            ft.wait();
            }
        }
    };



template<typename RangeT>
void 
contract(CProps const& p,
         RTenRefc<RangeT> A, 
         RTenRefc<RangeT> B, 
         RTenRef<RangeT>  C)
    {
    //println();
    //println("------------------------------------------");
    //
    // Optimizations TODO
    //
    // o Allocate memory for newA, newB, and newC in a single 
    //   allocation. Use a helper object (with std::array of
    //   ptrs and sizes) to manage.
    //   This will also make newA-C into MatRef's, avoiding
    //   having to allocate any Ranges.
    // 

    //cpu_time cpu;

    MultAlloc<Real,3> alloc;
    alloc.add(p.permuteA() ? area(p.newArange) : 0);
    alloc.add(p.permuteB() ? area(p.newBrange) : 0);
    alloc.add(p.permuteC() ? area(p.newCrange) : 0);
    alloc.allocate();

    MatRefc aref;
    if(p.permuteA())
        {
        //println("Calling permute A");
        auto tref = makeTenRef(alloc[0],p.newArange);
        permute(A,p.PA,tref);
        aref = MatRefc(tref.data(),p.dmid,p.dleft);
        aref.applyTrans();
        }
    else
        {
        //println("A is matrix already, making aref");
        if(p.Atrans())
            {
            //println("  Transposing aref");
            aref = MatRefc(A.data(),p.dmid,p.dleft);
            aref.applyTrans();
            }
        else
            {
            aref = MatRefc(A.data(),p.dleft,p.dmid);
            }
        }

    MatRefc bref;
    if(p.permuteB())
        {
        //println("Calling permute B");
        auto tref = makeTenRef(alloc[1],p.newBrange);
        permute(B,p.PB,tref);
        bref = MatRefc(tref.data(),p.dmid,p.dright);
        }
    else
        {
        //println("B is matrix already, making bref");
        if(p.Btrans())
            {
            //println("  Transposing bref");
            bref = MatRefc(B.data(),p.dright,p.dmid);
            bref.applyTrans();
            }
        else
            {
            bref = MatRefc(B.data(),p.dmid,p.dright);
            }
        }

    //println("A and B permuted, took ",cpu.sincemark());

#ifdef DEBUG
    if(C.size() != aref.Nrows()*bref.Ncols())
        {
        println("C.size() = ",C.size());
        printfln("aref.Ncols()*bref.Nrows() = %d*%d = %d",aref.Ncols(),bref.Nrows(),aref.Ncols()*bref.Nrows());
        throw std::runtime_error("incorrect size of C in contract");
        }
#endif

    MatRef cref;
    RTenRef<decltype(p.newCrange)> newC;
    if(p.permuteC())
        {
        newC = makeTenRef(alloc[2],p.newCrange);
        cref = MatRef(newC.data(),aref.Nrows(),bref.Ncols());
        }
    else
        {
        if(p.Ctrans()) 
            {
            cref = MatRef(C.data(),bref.Ncols(),aref.Nrows());
            cref.applyTrans();
            }
        else
            {
            cref = MatRef(C.data(),aref.Nrows(),bref.Ncols());
            }
        }

    //cpu.mark();
    //printfln("Multiplying a %dx%d%s * %dx%d%s = %dx%d%s",
    //         aref.Nrows(),aref.Ncols(),aref.transposed()?"(t)":"",
    //         bref.Nrows(),bref.Ncols(),bref.transposed()?"(t)":"",
    //         cref.Nrows(),cref.Ncols(),cref.transposed()?"(t)":"");

    //Important that this be multAdd for block-sparse case
    //where multiple contractions can get added to same block
    //(e.g. when result is a scalar)
    multAdd(aref,bref,cref);


    //println("Matrix multiply done, took ",cpu.sincemark());

    if(p.permuteC())
        {
#ifdef DEBUG
        if(isTrivial(p.PC)) Error("Calling permute in contract with a trivial permutation");
#endif
        //cpu_time cpuC; 
        //if(Global::debug1()) printfln("Permuting C, P = %s, isTrivial = %s",p.PC,isTrivial(p.PC));
        permute(makeRefc(newC),p.PC,C,detail::plusEq<Real>);
        //println("C permuted, took ",cpuC.sincemark());
        }
    //println("------------------------------------------");
    //println();
    }

template<typename RangeT>
void 
contract(RTenRefc<RangeT> A, Label const& ai, 
         RTenRefc<RangeT> B, Label const& bi, 
         RTenRef<RangeT>  C, Label const& ci)
    {
    if(ai.empty())
        {
        permute(B,bi,C,ci);
        auto val = *A.data();
        for(auto& el : C) el *= val;
        return;
        }
    else if(bi.empty())
        {
        permute(A,ai,C,ci);
        auto val = *B.data();
        for(auto& el : C) el *= val;
        return;
        }
    CProps props(ai,bi,ci);
    props.compute(A,B,makeRefc(C));
    contract(props,A,B,C);
    }

//Explicit template instantiations:
template void 
contract(RTenRefc<Range>, Label const&, 
         RTenRefc<Range>, Label const&, 
         RTenRef<Range>,  Label const&);
template void 
contract(RTenRefc<IndexSet>, Label const&, 
         RTenRefc<IndexSet>, Label const&, 
         RTenRef<IndexSet>,  Label const&);


struct MultInfo
    {
    bool tA = false,
         tB = false,
         Bfirst = false;
    MultInfo() {} 
    };

MultInfo
computeMultInfo(Label const& ai,
                Label const& bi, 
                Label const& ci)
    {
    MultInfo I;
    if(ai[1] == ci[1])
        {
        if(ai[0] == bi[1])  // Bik Akj = Cij,  mC += mB * mA
            {
            I.Bfirst = true;
            //println("=======Case 1==========");
            }
        else //ai[0] == bi[0]) Bki Akj = C_ij,  mC += mBt * mA
            {
            I.tB = true;
            I.Bfirst = true;
            //println("=======Case 2==========");
            }
        }
    else if(ai[1] == ci[0])
        {
        if(ai[0] == bi[1])  // Aki Bjk = Cij,  mCt += mAt * mBt;
            {
            I.tA = true;
            I.tB = true;
            //println("=======Case 3==========");
            }
        else //ai[0] == bi[0]) Aki Bkj = Cij,  mC += mAt * mB
            {
            I.tA = true;
            //println("=======Case 4==========");
            }
        }
    else if(ai[1] == bi[1])
        {
        if(ai[0] == ci[1])  // Bik Ajk = Cij,  mC += mB * mAt
            {
            I.Bfirst = true;
            I.tA = true;
            //println("=======Case 5==========");
            }
        else //ai[0] == ci[0]) Aik Bjk = Cij,  mC += mA * mBt
            {
            I.tB = true;
            //println("=======Case 6==========");
            }
        }
    else if(ai[1] == bi[0])
        {
        if(ai[0] == ci[1]) // Bki Ajk = Cij, mC += mBt * mAt
            {
            I.Bfirst = true;
            I.tA = true;
            I.tB = true;
            //println("=======Case 7==========");
            }
        else //ai[0] == ci[0], Aik Bkj = Cij, mC += mA * mB
            {
            //println("=======Case 8==========");
            }
        }
    return I;
    }

template<typename RangeT>
void 
contractloop(RTenRefc<RangeT> A, Label const& ai, 
             RTenRefc<RangeT> B, Label const& bi, 
             RTenRef<RangeT>  C, Label const& ci,
             Args const& args)
    {
    if(ai.empty() || bi.empty())
        {
        contract(A,ai,B,bi,C,ci);
        return;
        }
    //println();
    //println("Loop start--------------------------------");
    CProps p(ai,bi,ci);
    p.computeNactive();
    //printfln("nactive A, B, C are %d %d %d",p.nactiveA,p.nactiveB,p.nactiveC);
    if(p.nactiveA != 2 || p.nactiveB != 2 || p.nactiveC != 2)
        {
        //println("calling contract from within contractloop");
        p.compute(A,B,makeRefc(C));
        contract(p,A,B,C);
        return;
        }
    p.computePerms();

    auto nthread = args.getInt("NThread",4);

    long ra = ai.size(),
         rb = bi.size(),
         rc = ci.size();

    //vector<long> cdims(rc);
    //for(int i = 0; i < ra; ++i)
    //    if(p.AtoC[i] >= 0)
    //        {
    //        cdims[p.AtoC[i]] = A.extent(i);
    //        }
    //for(int j = 0; j < rb; ++j)
    //    if(p.BtoC[j] >= 0)
    //        {
    //        cdims[p.BtoC[j]] = B.extent(j);
    //        }
    //C = Ten(cdims,0.);

    auto nfo = computeMultInfo(ai,bi,ci);

    auto Arow = A.extent(0), Acol = A.extent(1);
    auto Brow = B.extent(0), Bcol = B.extent(1);
    auto Crow = C.extent(0), Ccol = C.extent(1);

    detail::GCounter couA(ra), 
                     couB(rb);
    //Keep couA.i[0] and couA.i[1] fixed at 0
    couA.setRange(0,0,0);
    couA.setRange(1,0,0);
    //Let couA.i[j] (j >= 2) range from 0 to A.extent(j)-1
    for(int j = 2; j < ra; ++j)
        couA.setRange(j,0,A.extent(j)-1);

    //Similarly for couB
    couB.setRange(0,0,0);
    couB.setRange(1,0,0);
    for(int j = 2; j < rb; ++j)
        couB.setRange(j,0,B.extent(j)-1);

    Label aind(ra,0), 
          bind(rb,0), 
          cind(rc,0);

    CABqueue cabq;

    for(; couA.notDone(); ++couA)
        {
        for(int ia = 2; ia < ra; ++ia)
            {
            aind[ia] = couA[ia];
            }
        auto offA = ind(A,aind);

        //TODO: possible bug, shouldn't we
        //      call couB.setRange to extend
        //      ranges to 0...B.extent(j)-1
        //      for those which aren't restricted
        //      to ival below?
        couB.reset();
        for(int ia = 2; ia < ra; ++ia)
            {
            auto ival = couA[ia];
            if(p.contractedA(ia))
                {
                couB.setRange(p.AtoB(ia),ival,ival);
                }
            else
                {
                cind[p.AtoC(ia)] = ival;
                }
            }
        
        for(;couB.notDone(); ++couB)
            {
            for(int ib = 2; ib < rb; ++ib)
                {
                bind[ib] = couB[ib];
                if(p.BtoC(ib) != -1) 
                if(!p.contractedB(ib))
                    cind[p.BtoC(ib)] = couB[ib];
                }

            auto offB = ind(B,bind);
            auto offC = ind(C,cind);

            auto sA = MatRefc(A.data()+offA,Arow,Acol);
            if(nfo.tA) sA.applyTrans();
            auto sB = MatRefc(B.data() + offB,Brow,Bcol);
            if(nfo.tB) sB.applyTrans();
            auto sC = MatRef(C.data()+offC,Crow,Ccol);

            if(nfo.Bfirst)
                {
                //mult_add(sB,sA,sC);
                cabq.addtask(sB,sA,sC,offC+1);
                }
            else
                {
                //mult_add(sA,sB,sC);
                cabq.addtask(sA,sB,sC,offC+1);
                }
            }
        }
    cabq.run(nthread);
    //println("Loop end----------------------------------");
    //println();
    }
template
void 
contractloop(RTenRefc<Range> A, Label const& ai, 
             RTenRefc<Range> B, Label const& bi, 
             RTenRef<Range>  C, Label const& ci,
             Args const& args);
template
void 
contractloop(RTenRefc<IndexSet> A, Label const& ai, 
             RTenRefc<IndexSet> B, Label const& bi, 
             RTenRef<IndexSet>  C, Label const& ci,
             Args const& args);


////////////////////////////////////////

//All indices of B contracted
//(A can have some uncontracted indices)
template<typename RangeT>
void 
contractDiagFull(VecRefc A,         Label const& ai, 
                 RTenRefc<RangeT> B, Label const& bi, 
                 VecRef          C, Label const& ci)
    {
    Error("contractDiagFull not yet implemented");
    }
template
void 
contractDiagFull(VecRefc A,        Label const& ai, 
                 RTenRefc<Range> B, Label const& bi, 
                 VecRef         C, Label const& ci);
template
void 
contractDiagFull(VecRefc A,           Label const& ai, 
                 RTenRefc<IndexSet> B, Label const& bi, 
                 VecRef            C, Label const& ci);

////////////////////////////////////////////


} //namespace itensor