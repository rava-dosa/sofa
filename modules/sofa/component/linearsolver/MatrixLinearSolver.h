/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, version 1.0 beta 4      *
*                (c) 2006-2009 MGH, INRIA, USTL, UJF, CNRS                    *
*                                                                             *
* This library is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This library is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this library; if not, write to the Free Software Foundation,     *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.          *
*******************************************************************************
*                               SOFA :: Modules                               *
*                                                                             *
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
#ifndef SOFA_COMPONENT_LINEARSOLVER_MATRIXLINEARSOLVER_H
#define SOFA_COMPONENT_LINEARSOLVER_MATRIXLINEARSOLVER_H

#include <sofa/simulation/common/SolverImpl.h>
#include <sofa/simulation/common/MechanicalVisitor.h>
#include <sofa/core/behavior/LinearSolver.h>
#include <sofa/component/component.h>
#include <sofa/component/linearsolver/DefaultMultiMatrixAccessor.h>
#include <sofa/component/linearsolver/SparseMatrix.h>
#include <sofa/component/linearsolver/FullMatrix.h>
#include <sofa/component/linearsolver/GraphScatteredTypes.h>

namespace sofa
{

namespace component
{

namespace linearsolver
{

class MatrixInvertData {};

template<class Matrix, class Vector>
class SOFA_EXPORT_DYNAMIC_LIBRARY BaseMatrixLinearSolver : public sofa::core::behavior::LinearSolver
{
public:
    SOFA_CLASS(SOFA_TEMPLATE2(BaseMatrixLinearSolver,Matrix,Vector), sofa::core::behavior::LinearSolver);

    virtual void invert(Matrix& M) = 0;

    virtual void solve(Matrix& M, Vector& solution, Vector& rh) = 0;
};


template<class Matrix, class Vector>
class MatrixLinearSolverInternalData
{
public:
    MatrixLinearSolverInternalData(core::objectmodel::BaseObject*)
    {}
};

template<class Matrix, class Vector>
class SOFA_EXPORT_DYNAMIC_LIBRARY MatrixLinearSolver : public BaseMatrixLinearSolver<Matrix, Vector>, public sofa::simulation::SolverImpl
{
public:
    SOFA_CLASS2(SOFA_TEMPLATE2(MatrixLinearSolver,Matrix,Vector), SOFA_TEMPLATE2(BaseMatrixLinearSolver,Matrix,Vector), sofa::simulation::SolverImpl);

    typedef sofa::core::behavior::BaseMechanicalState::VecId VecId;
    typedef  std::list<int> ListIndex;

    Data<bool> multiGroup;

    MatrixLinearSolver();
    virtual ~MatrixLinearSolver();

    /// Reset the current linear system.
    void resetSystem();

    /// Reset the current linear system.
    void resizeSystem(int n);

    /// Set the linear system matrix, combining the mechanical M,B,K matrices using the given coefficients
    ///
    /// Note that this automatically resizes the linear system to the number of active degrees of freedoms
    ///
    /// @todo Should we put this method in a specialized class for mechanical systems, or express it using more general terms (i.e. coefficients of the second order ODE to solve)
    void setSystemMBKMatrix(double mFact=0.0, double bFact=0.0, double kFact=0.0);

    /// Set the linear system right-hand term vector, from the values contained in the (Mechanical/Physical)State objects
    void setSystemRHVector(VecId v);

    /// Set the initial estimate of the linear system left-hand term vector, from the values contained in the (Mechanical/Physical)State objects
    /// This vector will be replaced by the solution of the system once solveSystem is called
    void setSystemLHVector(VecId v);

    /// Get the linear system matrix, or NULL if this solver does not build it
    Matrix* getSystemMatrix() { return currentGroup->systemMatrix; }

    /// Get the linear system right-hand term vector, or NULL if this solver does not build it
    Vector* getSystemRHVector() { return currentGroup->systemRHVector; }

    /// Get the linear system left-hand term vector, or NULL if this solver does not build it
    Vector* getSystemLHVector() { return currentGroup->systemLHVector; }

    /// Get the linear system matrix, or NULL if this solver does not build it
    defaulttype::BaseMatrix* getSystemBaseMatrix() { return currentGroup->systemMatrix; }

    /// Get the linear system right-hand term vector, or NULL if this solver does not build it
    defaulttype::BaseVector* getSystemRHBaseVector() { return currentGroup->systemRHVector; }

    /// Get the linear system left-hand term vector, or NULL if this solver does not build it
    defaulttype::BaseVector* getSystemLHBaseVector() { return currentGroup->systemLHVector; }

    /// Solve the system as constructed using the previous methods
    virtual void solveSystem();

    /// Invert the system, this method is optional because it's call when solveSystem() is called for the first time
    virtual void invertSystem()
    {
        for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
        {
            if (!isMultiSolve()) setGroup(g);
            if (currentGroup->needInvert)
            {
                this->invert(*currentGroup->systemMatrix);
                currentGroup->needInvert = false;
            }
        }
    }

    virtual std::string getTemplateName() const
    {
        return templateName(this);
    }

    static std::string templateName(const MatrixLinearSolver<Matrix,Vector>* = NULL)
    {
        return Matrix::Name();
    }


    virtual void invert(Matrix& /*M*/) {}

    virtual void solve(Matrix& M, Vector& solution, Vector& rh) = 0;


    /// Default implementation of Multiply the inverse of the system matrix by the transpose of the given matrix, and multiply the result with the given matrix J
    ///
    /// TODO : put this implementation in MatrixLinearSolver class - fix problems with Scattered Matrix

    template<class RMatrix, class JMatrix>
    bool addJMInvJt(RMatrix& result, JMatrix& J, double fact)
    {
        const unsigned int Jrows = J.rowSize();
        const unsigned int Jcols = J.colSize();
        if (Jcols != currentGroup->systemMatrix->rowSize())
        {
            serr << "LULinearSolver::addJMInvJt ERROR: incompatible J matrix size." << sendl;
            return false;
        }

        if (!Jrows) return false;

        const typename JMatrix::LineConstIterator jitend = J.end();
        // STEP 1 : put each line of matrix Jt in the right hand term of the system
        for (typename JMatrix::LineConstIterator jit1 = J.begin(); jit1 != jitend; ++jit1)
        {
            int row1 = jit1->first;
            // clear the right hand term:
            currentGroup->systemRHVector->clear(); // currentGroup->systemMatrix->rowSize()
            //double acc = 0.0;
            for (typename JMatrix::LElementConstIterator i1 = jit1->second.begin(), i1end = jit1->second.end(); i1 != i1end; ++i1)
            {
                currentGroup->systemRHVector->add(i1->first,i1->second);
            }

            // STEP 2 : solve the system :
            solveSystem();


            // STEP 3 : project the result using matrix J
            for (typename JMatrix::LineConstIterator jit2 = jit1; jit2 != jitend; ++jit2)
            {
                int row2 = jit2->first;
                double acc = 0.0;
                for (typename JMatrix::LElementConstIterator i2 = jit2->second.begin(), i2end = jit2->second.end(); i2 != i2end; ++i2)
                {
                    int col2 = i2->first;
                    double val2 = i2->second;
                    acc += val2 * currentGroup->systemLHVector->element(col2);
                }
                acc *= fact;
                //sout << "W("<<row1<<","<<row2<<") += "<<acc<<" * "<<fact<<sendl;
                result.add(row2,row1,acc);
                if (row1!=row2)
                    result.add(row1,row2,acc);
            }
        }
        return true;
    }





    /// Default implementation of Multiply the inverse of the system matrix by the transpose of the given matrix, and multiply the result with the given matrix J
    ///
    /// @param result the variable where the result will be added
    /// @param J the matrix J to use
    /// @return false if the solver does not support this operation, of it the system matrix is not invertible
    bool addJMInvJt(defaulttype::BaseMatrix* result, defaulttype::BaseMatrix* J, double fact)
    {
        if (FullMatrix<double>* r = dynamic_cast<FullMatrix<double>*>(result))
        {
            if (SparseMatrix<double>* j = dynamic_cast<SparseMatrix<double>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
            else if (SparseMatrix<float>* j = dynamic_cast<SparseMatrix<float>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
        }
        else if (FullMatrix<double>* r = dynamic_cast<FullMatrix<double>*>(result))
        {
            if (SparseMatrix<double>* j = dynamic_cast<SparseMatrix<double>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
            else if (SparseMatrix<float>* j = dynamic_cast<SparseMatrix<float>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
        }
        else if (defaulttype::BaseMatrix* r = result)
        {
            if (SparseMatrix<double>* j = dynamic_cast<SparseMatrix<double>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
            else if (SparseMatrix<float>* j = dynamic_cast<SparseMatrix<float>*>(J))
            {
                return addJMInvJt(*r,*j,fact);
            }
        }
        return false;
    }

    bool isMultiGroup() const
    {
        return multiGroup.getValue();
    }

    /// Returns true if this implementation can handle all integration groups at once
    virtual bool isMultiSolve() const
    {
        return false;
    }

    virtual simulation::MultiNodeDataMap* getNodeMap()
    {
        if (isMultiGroup()) return &this->nodeMap;
        else                       return NULL;
    }

    virtual simulation::MultiNodeDataMap* getWriteNodeMap()
    {
        if (isMultiGroup()) return &this->writeNodeMap;
        else                       return NULL;
    }

    virtual void createGroups();

    int getNbGroups() const
    {
        if (isMultiGroup()) return this->groups.size();
        else return 1;
    }

    void setGroup(int i)
    {
        //serr << "setGroup("<<i<<")" << sendl;
        if (isMultiGroup() && (unsigned)i < this->groups.size())
        {
            currentNode = groups[i];
            currentGroup = &(gData[currentNode]);
            nodeMap.clear();
            nodeMap[currentNode] = 1.0;
            writeNodeMap.clear();
            writeNodeMap[currentNode] = 0.0;
        }
        else
        {
            currentNode = dynamic_cast<simulation::Node*>(this->getContext());
            currentGroup = &defaultGroup;
            nodeMap.clear();
            writeNodeMap.clear();
        }
    }

    double multiv_dot(VecId a, VecId b, helper::vector<double>& res)
    {
        this->v_dot(a,b);
        finish();
        res.resize(groups.size());
        for (unsigned int g=0; g<groups.size(); ++g)
        {
            simulation::MultiNodeDataMap::const_iterator it = writeNodeMap.find(groups[g]);
            if (it == writeNodeMap.end())
                res[g] = 0.0;
            else
                res[g] = it->second;
        }
        return result;
    }

protected:

    /// newPartially solve the system
    virtual void partial_solve(Matrix& /*M*/, Vector& /*partial_solution*/, Vector& /*sparse_rh*/, ListIndex& /* indices_solution*/, ListIndex& /* indices input */) {}

    Vector* createVector();
    static void deleteVector(Vector* v);

    Matrix* createMatrix();
    static void deleteMatrix(Matrix* v);

    MatrixLinearSolverInternalData<Matrix,Vector>* data;
    MatrixInvertData * invertData;

    MatrixInvertData * getMatrixInvertData(Matrix * m);
    virtual MatrixInvertData * createInvertData();

    simulation::MultiNodeDataMap nodeMap;
    simulation::MultiNodeDataMap writeNodeMap;

    class GroupData
    {
    public:
        int systemSize;
        bool needInvert;
        Matrix* systemMatrix;
        Vector* systemRHVector;
        Vector* systemLHVector;
        VecId solutionVecId;
        DefaultMultiMatrixAccessor matrixAccessor;
        GroupData()
            : systemSize(0), needInvert(true), systemMatrix(NULL), systemRHVector(NULL), systemLHVector(NULL)
        {}
        ~GroupData()
        {
            if (systemMatrix) deleteMatrix(systemMatrix);
            if (systemRHVector) deleteVector(systemRHVector);
            if (systemLHVector) deleteVector(systemLHVector);
        }
    };
    typedef std::map<simulation::Node*,GroupData> GroupDataMap;
    typedef typename GroupDataMap::iterator GroupDataMapIter;
    simulation::Node* currentNode;
    GroupData* currentGroup;
    std::vector<simulation::Node*> groups;
    GroupDataMap gData;
    GroupData defaultGroup;

};

template<class Matrix, class Vector>
MatrixLinearSolver<Matrix,Vector>::MatrixLinearSolver()
    : multiGroup( initData( &multiGroup, false, "multiGroup", "activate multiple system solve, one for each child node" ) )
//, needInvert(true), systemMatrix(NULL), systemRHVector(NULL), systemLHVector(NULL)
    , currentGroup(&defaultGroup)
{
    invertData = NULL;
    data = new MatrixLinearSolverInternalData<Matrix,Vector>(this);
}

template<class Matrix, class Vector>
MatrixLinearSolver<Matrix,Vector>::~MatrixLinearSolver()
{
    //if (systemMatrix) deleteMatrix(systemMatrix);
    //if (systemRHVector) deleteVector(systemRHVector);
    //if (systemLHVector) deleteVector(systemLHVector);
    if (invertData) delete invertData;
    invertData = NULL;
}

template<class Matrix, class Vector>
MatrixInvertData * MatrixLinearSolver<Matrix,Vector>::getMatrixInvertData(Matrix * m)
{
    if (invertData==NULL) invertData=createInvertData();
    return invertData;
}

template<class Matrix, class Vector>
MatrixInvertData * MatrixLinearSolver<Matrix,Vector>::createInvertData()
{
    std::cerr << "ERROR the solver didn't implement MatrixLinearSolver::getMatrixInvertData this function is not available in MatrixLinearSolver, NULL is return" << std::endl;
    return NULL;
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::createGroups()
{
    simulation::Node* root = dynamic_cast<simulation::Node*>(this->getContext());
    //defaultGroup.node = root;
    nodeMap.clear();
    writeNodeMap.clear();
    for (GroupDataMapIter it = gData.begin(), itend = gData.end(); it != itend; ++it)
        it->second.systemSize = 0;
    if (isMultiGroup())
    {
        for (unsigned int g=0; g<root->child.size(); ++g)
        {
            simulation::Node* n = root->child[g];
            nodeMap[n] = 0.0;
        }

        double dim = 0;
        simulation::MechanicalGetDimensionVisitor(&dim).setNodeMap(&nodeMap).execute(root);

        groups.clear();

        for (unsigned int g=0; g<root->child.size(); ++g)
        {
            simulation::Node* n = root->child[g];
            double gdim = nodeMap[ n ];
            if (gdim <= 0) continue;
            groups.push_back(n);
            gData[n].systemSize = (int)gdim;
            dim += gdim;
        }

        defaultGroup.systemSize = (int)dim;

        // set nodemap to default (i.e. factor 1 for all non empty groups
        nodeMap.clear();
        writeNodeMap.clear();
        for (unsigned int g=0; g<groups.size(); ++g)
        {
            nodeMap[ groups[g] ] = 1.0;
            writeNodeMap[ groups[g] ] = 0.0;
        }
    }
    else
    {
        groups.clear();
        double dim = 0;
        simulation::MechanicalGetDimensionVisitor(&dim).execute(root);
        defaultGroup.systemSize = (int)dim;
    }
    currentNode = root;
    currentGroup = &defaultGroup;
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::resetSystem()
{
    for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
    {
        if (!isMultiSolve()) setGroup(g);
        if (!this->frozen)
        {
            if (currentGroup->systemMatrix) currentGroup->systemMatrix->clear();
            currentGroup->needInvert = true;
        }
        if (currentGroup->systemRHVector) currentGroup->systemRHVector->clear();
        if (currentGroup->systemLHVector) currentGroup->systemLHVector->clear();
        currentGroup->solutionVecId = VecId();
    }
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::resizeSystem(int n)
{
    if (!this->frozen)
    {
        if (!currentGroup->systemMatrix) currentGroup->systemMatrix = createMatrix();
        currentGroup->systemMatrix->resize(n,n);
    }
    if (!currentGroup->systemRHVector) currentGroup->systemRHVector = createVector();
    currentGroup->systemRHVector->resize(n);
    if (!currentGroup->systemLHVector) currentGroup->systemLHVector = createVector();
    currentGroup->systemLHVector->resize(n);
    currentGroup->needInvert = true;
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::setSystemMBKMatrix(double mFact, double bFact, double kFact)
{
    createGroups();
    for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
    {
        if (!isMultiSolve()) setGroup(g);
        if (!this->frozen)
        {
            if (!currentGroup->systemMatrix) currentGroup->systemMatrix = createMatrix();
            currentGroup->matrixAccessor.setGlobalMatrix(currentGroup->systemMatrix);
            currentGroup->matrixAccessor.clear();
            //unsigned int nbRow=0, nbCol=0;
            //MechanicalGetMatrixDimensionVisitor(nbRow, nbCol).execute( getContext() );
            //this->getMatrixDimension(&nbRow, &nbCol);
            //resizeSystem(nbRow);
            this->getMatrixDimension(&(currentGroup->matrixAccessor));
            currentGroup->matrixAccessor.setupMatrices();
            resizeSystem(currentGroup->matrixAccessor.getGlobalDimension());
            currentGroup->systemMatrix->clear();
            //unsigned int offset = 0;
            //MechanicalAddMBK_ToMatrixVisitor(currentGroup->systemMatrix, mFact, bFact, kFact, offset).execute( getContext() );
            this->addMBK_ToMatrix(&(currentGroup->matrixAccessor), mFact, bFact, kFact);
            currentGroup->matrixAccessor.computeGlobalMatrix();
        }
    }
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::setSystemRHVector(VecId v)
{
    for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
    {
        if (!isMultiSolve()) setGroup(g);
        this->multiVector2BaseVector(v, currentGroup->systemRHVector, &(currentGroup->matrixAccessor));
    }
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::setSystemLHVector(VecId v)
{
    for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
    {
        if (!isMultiSolve()) setGroup(g);
        currentGroup->solutionVecId = v;
        //unsigned int offset = 0;
        //MechanicalMultiVector2BaseVectorVisitor(v, systemLHVector, offset).execute( getContext() );
        this->multiVector2BaseVector(v, currentGroup->systemLHVector, &(currentGroup->matrixAccessor));
    }
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::solveSystem()
{
    for (unsigned int g=0, nbg = isMultiSolve() ? 1 : getNbGroups(); g < nbg; ++g)
    {
        if (!isMultiSolve()) setGroup(g);
        if (currentGroup->needInvert)
        {
            this->invert(*currentGroup->systemMatrix);
            currentGroup->needInvert = false;
        }
        this->solve(*currentGroup->systemMatrix, *currentGroup->systemLHVector, *currentGroup->systemRHVector);
        if (!currentGroup->solutionVecId.isNull())
        {
            //unsigned int offset = 0;
            v_clear(currentGroup->solutionVecId);
            multiVectorPeqBaseVector(currentGroup->solutionVecId, currentGroup->systemLHVector, &(currentGroup->matrixAccessor));
        }
    }
}

/*

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::partialSolveSystem(VecIndex&  Iout, VecIndex&  Iin)
{
    if (needInvert)
    {
        this->invert(*systemMatrix);
        needInvert = false;
    }
    this->partial_solve(*systemMatrix, *systemLHVector, *systemRHVector, Iout, Iin);


	if (!solutionVecId.isNull())
    {
        unsigned int offset = 0;
        //MechanicalBaseVector2MultiVectorVisitor(systemLHVector, solutionVecId, offset).execute( getContext() );
        //MechanicalVOpVisitor(solutionVecId).execute( getContext() ); // clear solutionVecId
        //MechanicalMultiVectorPeqBaseVectorVisitor(solutionVecId, systemLHVector, offset).execute( getContext() );
        v_clear(solutionVecId);
        multiVectorPeqBaseVector(solutionVecId, systemLHVector, offset);
    }
}
*/

template<class Matrix, class Vector>
Vector* MatrixLinearSolver<Matrix,Vector>::createVector()
{
    return new Vector;
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::deleteVector(Vector* v)
{
    delete v;
}

template<class Matrix, class Vector>
Matrix* MatrixLinearSolver<Matrix,Vector>::createMatrix()
{
    return new Matrix;
}

template<class Matrix, class Vector>
void MatrixLinearSolver<Matrix,Vector>::deleteMatrix(Matrix* v)
{
    delete v;
}

#if defined(WIN32) && !defined(SOFA_COMPONENT_LINEARSOLVER_MATRIXLINEARSOLVER_CPP)
extern template class SOFA_COMPONENT_LINEARSOLVER_API MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>;
#endif

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::resetSystem();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::resizeSystem(int);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::setSystemMBKMatrix(double mFact, double bFact, double kFact);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::setSystemRHVector(VecId v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::setSystemLHVector(VecId v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::solveSystem();

template<> SOFA_COMPONENT_LINEARSOLVER_API
GraphScatteredVector* MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::createVector();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::deleteVector(GraphScatteredVector* v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
GraphScatteredMatrix* MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::createMatrix();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::deleteMatrix(GraphScatteredMatrix* v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseMatrix* MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::getSystemBaseMatrix();

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseVector* MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::getSystemRHBaseVector();

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseVector* MatrixLinearSolver<GraphScatteredMatrix,GraphScatteredVector>::getSystemLHBaseVector();

#ifdef SOFA_SMP
template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::resetSystem();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::resizeSystem(int);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::setSystemMBKMatrix(double mFact, double bFact, double kFact);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::setSystemRHVector(VecId v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::setSystemLHVector(VecId v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::solveSystem();

template<> SOFA_COMPONENT_LINEARSOLVER_API
ParallelGraphScatteredVector* MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::createVector();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::deleteVector(ParallelGraphScatteredVector* v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
GraphScatteredMatrix* MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::createMatrix();

template<> SOFA_COMPONENT_LINEARSOLVER_API
void MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::deleteMatrix(GraphScatteredMatrix* v);

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseMatrix* MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::getSystemBaseMatrix();

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseVector* MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::getSystemRHBaseVector();

template<> SOFA_COMPONENT_LINEARSOLVER_API
defaulttype::BaseVector* MatrixLinearSolver<GraphScatteredMatrix,ParallelGraphScatteredVector>::getSystemLHBaseVector();
#endif

} // namespace linearsolver

} // namespace component

} // namespace sofa

#endif
