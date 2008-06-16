#ifndef SOFA_COMPONENT_TOPOLOGY_EDGESETTOPOLOGY_INL
#define SOFA_COMPONENT_TOPOLOGY_EDGESETTOPOLOGY_INL

#include <sofa/component/topology/EdgeSetTopology.h>
#include <sofa/component/topology/PointSetTopology.inl>
#include <sofa/component/topology/TopologyChangedEvent.h>
#include <sofa/simulation/common/PropagateEventVisitor.h>
#include <sofa/simulation/tree/GNode.h>
#include <algorithm>
#include <functional>

// Use BOOST GRAPH LIBRARY :

#include <boost/config.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>

#include <boost/graph/cuthill_mckee_ordering.hpp>
#include <boost/graph/bandwidth.hpp>

namespace sofa
{

namespace component
{

namespace topology
{

using namespace sofa::defaulttype;
using namespace sofa::core::componentmodel::behavior;



/////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////EdgeSetTopologyModifier//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////



template<class DataTypes>
class EdgeSetTopologyLoader : public PointSetTopologyLoader<DataTypes>
{
public:
    typedef typename DataTypes::VecCoord VecCoord;
    typedef typename DataTypes::Coord Coord;
    typedef typename DataTypes::Real Real;

    VecCoord pointArray;
    EdgeSetTopologyModifier<DataTypes> *estm;

    EdgeSetTopologyLoader(EdgeSetTopologyModifier<DataTypes> *tm) :PointSetTopologyLoader<DataTypes>(), estm(tm)
    {
    }

    virtual void addLine(int p1, int p2)
    {
        estm->addEdge(Edge((unsigned int)p1,(unsigned int)p2));
    }
};

template<class DataTypes>
void EdgeSetTopologyModifier<DataTypes>::addEdge(Edge e)
{

    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>( topology->getTopologyContainer() );
    assert (container != 0);
    container->m_edge.push_back(e);

}
template<class DataTypes>
bool EdgeSetTopologyModifier<DataTypes>::load(const char *filename)
{

    EdgeSetTopologyLoader<DataTypes> loader(this);
    if (!loader.load(filename))
        return false;
    else
    {
        loadPointSet(&loader);
        return true;
    }
}

template<class DataTypes>
void EdgeSetTopologyModifier<DataTypes>::addEdgesProcess(const sofa::helper::vector< Edge > &edges)
{
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>( topology->getTopologyContainer() );
    assert (container != 0);
    if (container->m_edge.size()>0)
    {
        const sofa::helper::vector< sofa::helper::vector<unsigned int> > &sa=container->getEdgeVertexShellArray();
        for (unsigned int i = 0; i < edges.size(); ++i)
        {
            const Edge &e = edges[i];
            // check if the 2 vertices are different
            assert(e[0]!=e[1]);
            // check if there already exists an edge
            assert(container->getEdgeIndex(e[0],e[1])== -1);
            container->m_edge.push_back(e);

            if (sa.size()>0)
            {

                sofa::helper::vector< unsigned int > &shell0 = container->getEdgeVertexShellForModification( e[0] );
                shell0.push_back( container->m_edge.size() - 1 );
                sort(shell0.begin(), shell0.end());

                sofa::helper::vector< unsigned int > &shell1 = container->getEdgeVertexShellForModification( e[1] );
                shell1.push_back( container->m_edge.size() - 1 );
                sort(shell1.begin(), shell1.end());
            }
        }
    }
}



template<class DataTypes>
void EdgeSetTopologyModifier<DataTypes>::addEdgesWarning(const unsigned int nEdges, const sofa::helper::vector< Edge >& edgesList,
        const sofa::helper::vector< unsigned int >& edgesIndexList,
        const sofa::helper::vector< sofa::helper::vector< unsigned int > > & ancestors,
        const sofa::helper::vector< sofa::helper::vector< double > >& baryCoefs)
{
    // Warning that edges just got created
    EdgesAdded *e=new EdgesAdded(nEdges, edgesList,edgesIndexList,ancestors,baryCoefs);
    this->addTopologyChange(e);
}




template<class DataTypes>
void EdgeSetTopologyModifier<DataTypes>::removeEdgesWarning( sofa::helper::vector<unsigned int> &edges )
{
    /// sort vertices to remove in a descendent order
    std::sort( edges.begin(), edges.end(), std::greater<unsigned int>() );

    // Warning that these edges will be deleted
    EdgesRemoved *e=new EdgesRemoved(edges);
    this->addTopologyChange(e);
}



template<class DataTypes>
void EdgeSetTopologyModifier<DataTypes>::removeEdgesProcess(const sofa::helper::vector<unsigned int> &indices,const bool removeIsolatedItems)
{
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>(topology->getTopologyContainer());
    assert (container != 0);

    if (removeIsolatedItems)
        container->getEdgeVertexShell(0);


    if (container->m_edge.size()>0)
    {
        sofa::helper::vector<unsigned int> vertexToBeRemoved;

        for (unsigned int i = 0; i < indices.size(); ++i)
        {
            Edge *e = &container->m_edge[ indices[i] ];
            unsigned int point0 = (*e)[0], point1 = (*e)[1];
            // first check that the edge shell array has been initialized
            if (container->m_edgeVertexShell.size()>0)
            {

                sofa::helper::vector< unsigned int > &shell0 = container->m_edgeVertexShell[ point0 ];
                // removes the first occurence (should be the only one) of the edge in the edge shell of the point
                //assert(std::find( shell0.begin(), shell0.end(), indices[i] ) !=shell0.end());
                shell0.erase( std::find( shell0.begin(), shell0.end(), indices[i] ) );
                if ((removeIsolatedItems) && (shell0.size()==0))
                {
                    vertexToBeRemoved.push_back(point0);
                }

                sofa::helper::vector< unsigned int > &shell1 = container->m_edgeVertexShell[ point1 ];
                // removes the first occurence (should be the only one) of the edge in the edge shell of the other point
                //assert(std::find( shell1.begin(), shell1.end(), indices[i] ) !=shell1.end());
                shell1.erase( std::find( shell1.begin(), shell1.end(), indices[i] ) );
                if ((removeIsolatedItems) && (shell1.size()==0))
                {
                    vertexToBeRemoved.push_back(point1);
                }
            }

            // removes the edge from the edgelist
            container->m_edge[ indices[i] ] = container->m_edge[ container->m_edge.size() - 1 ]; // overwriting with last valid value.
            container->m_edge.resize( container->m_edge.size() - 1 ); // resizing to erase multiple occurence of the edge.

            // now updates the shell information of the edge formely at the end of the array
            // first check that the edge shell array has been initialized
            // check if second test is necessary
            if ( indices[i] < container->m_edge.size() && (container->m_edgeVertexShell.size()>0))
            {

                unsigned int oldEdgeIndex=container->m_edge.size();
                e = &container->m_edge[ indices[i] ];
                point0 = (*e)[0]; point1 = (*e)[1];

                //replaces the edge index oldEdgeIndex with indices[i] for the first vertex
                sofa::helper::vector< unsigned int > &shell0 = container->m_edgeVertexShell[ point0 ];
                replace(shell0.begin(), shell0.end(), oldEdgeIndex, indices[i]);
                sort(shell0.begin(), shell0.end());

                //replaces the edge index oldEdgeIndex with indices[i] for the second vertex
                sofa::helper::vector< unsigned int > &shell1 = container->m_edgeVertexShell[ point1 ];
                replace(shell1.begin(), shell1.end(), oldEdgeIndex, indices[i]);
                sort(shell1.begin(), shell1.end());
            }
        }
        if (vertexToBeRemoved.size()>0)
        {
            this->removePointsWarning(vertexToBeRemoved);
            // inform other objects that the edges are going to be removed
            topology->propagateTopologicalChanges();
            this->removePointsProcess(vertexToBeRemoved);
        }
        //std::cout << "EdgeSetTopology: container has now "<<container->m_edge.size()<<" edges."<<std::endl;
    }
}


template<class DataTypes >
void EdgeSetTopologyModifier< DataTypes >::addPointsProcess(const unsigned int nPoints,
        const sofa::helper::vector< sofa::helper::vector< unsigned int > >& ancestors,
        const sofa::helper::vector< sofa::helper::vector< double > >& baryCoefs, const bool addDOF)
{

    // start by calling the standard method.
    PointSetTopologyModifier< DataTypes >::addPointsProcess( nPoints, ancestors, baryCoefs, addDOF );

    // now update the local container structures.
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>(topology->getTopologyContainer());
    assert (container != 0);
    container->m_edgeVertexShell.resize( container->m_edgeVertexShell.size() + nPoints );

}

template<class DataTypes >
void EdgeSetTopologyModifier< DataTypes >::addNewPoint(unsigned int i, const sofa::helper::vector< double >& x)
{

    // start by calling the standard method.
    PointSetTopologyModifier< DataTypes >::addNewPoint(i,x);

    // now update the local container structures.
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>(topology->getTopologyContainer());
    assert (container != 0);
    container->m_edgeVertexShell.resize( i+1 );

}



template< class DataTypes >
void EdgeSetTopologyModifier< DataTypes >::removePointsProcess( sofa::helper::vector<unsigned int> &indices, const bool removeDOF)
{
    // Important : the points are actually deleted from the mechanical object's state vectors iff (removeDOF == true)

    // now update the local container structures
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>(topology->getTopologyContainer());
    assert (container != 0);

    // forces the construction of the edge shell array if it does not exists
    //if (container->m_edge.size()>0)
    container->getEdgeVertexShellArray();

    // start by calling the standard method.
    PointSetTopologyModifier< DataTypes >::removePointsProcess( indices, removeDOF );



    unsigned int lastPoint = container->m_edgeVertexShell.size() - 1;

    for (unsigned int i = 0; i < indices.size(); ++i)
    {
        // updating the edges connected to the point replacing the removed one:
        // for all edges connected to the last point
        for (unsigned int j = 0; j < container->m_edgeVertexShell[lastPoint].size(); ++j)
        {
            // change the old index for the new one
            if ( container->m_edge[ container->m_edgeVertexShell[lastPoint][j] ][0] == lastPoint )
                container->m_edge[ container->m_edgeVertexShell[lastPoint][j] ][0] = indices[i];
            else
                container->m_edge[ container->m_edgeVertexShell[lastPoint][j] ][1] = indices[i];
        }

        // updating the edge shell itself (change the old index for the new one)
        container->m_edgeVertexShell[ indices[i] ] = container->m_edgeVertexShell[ lastPoint ];

        --lastPoint;
    }

    container->m_edgeVertexShell.resize( container->m_edgeVertexShell.size() - indices.size() );

}



template< class DataTypes >
void EdgeSetTopologyModifier< DataTypes >::renumberPointsProcess( const sofa::helper::vector<unsigned int> &index, const sofa::helper::vector<unsigned int> &inv_index, const bool renumberDOF)
{

    // now update the local container structures.
    EdgeSetTopology<DataTypes> *topology = dynamic_cast<EdgeSetTopology<DataTypes> *>(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast<EdgeSetTopologyContainer *>(topology->getTopologyContainer());
    assert (container != 0);

    container->getEdgeVertexShellArray();
    // start by calling the standard method
    PointSetTopologyModifier< DataTypes >::renumberPointsProcess( index, inv_index, renumberDOF );

    sofa::helper::vector< sofa::helper::vector< unsigned int > > EdgeVertexShell_cp = container->m_edgeVertexShell;
    for (unsigned int i = 0; i < index.size(); ++i)
    {
        container->m_edgeVertexShell[i] = EdgeVertexShell_cp[ index[i] ];
    }

    for (unsigned int i = 0; i < container->m_edge.size(); ++i)
    {
        unsigned int p0 = inv_index[ container->m_edge[i][0]  ];
        unsigned int p1 = inv_index[ container->m_edge[i][1]  ];

        if(p0<p1)
        {
            container->m_edge[i][0]  = p0;
            container->m_edge[i][1] = p1;
        }
        else
        {
            container->m_edge[i][0]  = p1;
            container->m_edge[i][1] = p0;
        }
    }


}



template<class DataTypes>
void EdgeSetTopologyModifier< DataTypes >::swapEdgesProcess(const sofa::helper::vector< sofa::helper::vector< unsigned int > >& edgesPairs)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast< EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert(topology != 0);

    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    EdgeSetTopologyModifier< DataTypes >* modifier  =this;
    assert(container != 0 );

    // first create the edges
    sofa::helper::vector< Edge > v;
    sofa::helper::vector< unsigned int > edgeIndexList;
    sofa::helper::vector<sofa::helper::vector<unsigned int> > ancestorsArray;
    unsigned int nbEdges=container->getNumberOfEdges();

    for (unsigned int i = 0; i < edgesPairs.size(); ++i)
    {
        unsigned int i1 = edgesPairs[i][0],
                     i2 = edgesPairs[i][1];

        unsigned int p11 = container->getEdge(i1)[0],
                     p12 = container->getEdge(i1)[1],
                     p21 = container->getEdge(i2)[0],
                     p22 = container->getEdge(i2)[1];

        Edge e1 (p11, p21),
             e2 (p12, p22);
        v.push_back(e1);
        v.push_back(e2);
        edgeIndexList.push_back(nbEdges);
        edgeIndexList.push_back(nbEdges+1);
        nbEdges+=2;
        sofa::helper::vector<unsigned int> ancestors;
        ancestors.push_back(i1);
        ancestors.push_back(i2);
        ancestorsArray.push_back(ancestors);

    }

    modifier->addEdgesProcess( v );

    // now warn about the creation
    modifier->addEdgesWarning( v.size(), v,edgeIndexList,ancestorsArray);

    //   EdgesAdded ea( 2 * edgesPairs.size(), v );
    // addTopologyChange( ea );

    // now warn about the destruction of the old edges
    sofa::helper::vector< unsigned int > indices;
    for (unsigned int i = 0; i < edgesPairs.size(); ++i)
    {
        indices.push_back( edgesPairs[i][0]  );
        indices.push_back( edgesPairs[i][1] );
    }
    modifier->removeEdgesWarning(indices );

    //            EdgesRemoved er( indices );
    //            addTopologyChange( er );

    // propagate the warnings
    this->m_basicTopology->propagateTopologicalChanges();


    // now destroy the old edges.
    modifier->removeEdgesProcess(  indices );

}

template<class DataTypes>
void EdgeSetTopologyModifier< DataTypes >::fuseEdgesProcess(const sofa::helper::vector< sofa::helper::vector< unsigned int > >& edgesPairs, const bool removeIsolatedPoints)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast< EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert(topology != 0);

    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    EdgeSetTopologyModifier< DataTypes >* modifier  =this;
    assert(container != 0 );

    // first create the edges
    sofa::helper::vector< Edge > v;
    sofa::helper::vector< unsigned int > edgeIndexList;
    sofa::helper::vector<sofa::helper::vector<unsigned int> > ancestorsArray;
    unsigned int nbEdges=container->getNumberOfEdges();

    for (unsigned int i = 0; i < edgesPairs.size(); ++i)
    {
        unsigned int i1 = edgesPairs[i][0],
                     i2 = edgesPairs[i][1];

        unsigned int p11 = container->getEdge(i1)[0],
                     p22 = container->getEdge(i2)[1];

        if(p11 == p22)
        {
            p11 = container->getEdge(i2)[0];
            p22 = container->getEdge(i1)[1];
        }

        Edge e (p11, p22);
        v.push_back(e);

        edgeIndexList.push_back(nbEdges);
        nbEdges+=1;
        sofa::helper::vector<unsigned int> ancestors;
        ancestors.push_back(i1);
        ancestors.push_back(i2);
        ancestorsArray.push_back(ancestors);

    }

    modifier->addEdgesProcess( v );

    // now warn about the creation
    modifier->addEdgesWarning( v.size(), v,edgeIndexList,ancestorsArray);

    // now warn about the destruction of the old edges
    sofa::helper::vector< unsigned int > indices;
    for (unsigned int i = 0; i < edgesPairs.size(); ++i)
    {
        indices.push_back( edgesPairs[i][0]  );
        indices.push_back( edgesPairs[i][1] );
    }

    modifier->removeEdgesWarning(indices );

    // propagate the warnings
    this->m_basicTopology->propagateTopologicalChanges();

    // now destroy the old edges.
    modifier->removeEdgesProcess(  indices, removeIsolatedPoints );

}



template<class DataTypes>
void EdgeSetTopologyModifier< DataTypes >::splitEdgesProcess( sofa::helper::vector<unsigned int> &indices,
        const sofa::helper::vector< sofa::helper::vector< double > >& baryCoefs, const bool removeIsolatedPoints)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    EdgeSetTopologyModifier< DataTypes >* modifier  = this;
    assert(container != 0);

    sofa::helper::vector< sofa::helper::vector< double > > defaultBaryCoefs;

    sofa::helper::vector< sofa::helper::vector< unsigned int > > v;
    v.resize(indices.size());
    sofa::helper::vector< Edge >  edges;
    sofa::helper::vector< unsigned int >  edgesIndex;
    unsigned int nbEdges = container->getNumberOfEdges();

    for (unsigned int i = 0; i < indices.size(); ++i)
    {
        unsigned int p1 = container->getEdge( indices[i] )[0],
                     p2 = container->getEdge( indices[i] )[1];

        // Adding the new point
        v[i].push_back( p1 );
        v[i].push_back( p2 );

        // Adding the new Edges
        Edge e1( p1, topology->getDOFNumber() + i ),
             e2( topology->getDOFNumber() + i, p2 );
        edges.push_back( e1 );
        edges.push_back( e2 );
        edgesIndex.push_back(nbEdges);
        edgesIndex.push_back(nbEdges+1);
        nbEdges+=2;

    }
    if (baryCoefs.size()==0)
    {
        defaultBaryCoefs.resize(indices.size());
        for (unsigned int i = 0; i < indices.size(); ++i)
        {
            defaultBaryCoefs[i].push_back(0.5);
            defaultBaryCoefs[i].push_back(0.5);
        }
        modifier->addPointsProcess( indices.size(), v ,defaultBaryCoefs);

    }
    else
        modifier->addPointsProcess( indices.size(), v ,baryCoefs);
    modifier->addEdgesProcess( edges );


    // warn about added points and edges
    if (baryCoefs.size()==0)
        modifier->addPointsWarning( indices.size(), v ,defaultBaryCoefs);
    else
        modifier->addPointsWarning( indices.size(), v , baryCoefs);
    modifier->addEdgesWarning( edges.size(),edges,edgesIndex);

    // warn about old edges about to be removed
    modifier->removeEdgesWarning( indices);

    this->m_basicTopology->propagateTopologicalChanges();

    // Removing the old edges
    modifier->removeEdgesProcess( indices, removeIsolatedPoints );

}

template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::removeEdges(sofa::helper::vector< unsigned int >& edges, const bool removeIsolatedPoints)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    /// add the topological changes in the queue
    modifier->removeEdgesWarning(edges);
    // inform other objects that the edges are going to be removed
    topology->propagateTopologicalChanges();
    // now destroy the old edges.
    modifier->removeEdgesProcess( edges, removeIsolatedPoints );

    assert(topology->getEdgeSetTopologyContainer()->checkTopology());
}

template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::removeItems(sofa::helper::vector< unsigned int >& items)
{
    removeEdges(items);
}

template<class DataTypes>
void  EdgeSetTopologyAlgorithms<DataTypes>::renumberPoints( const sofa::helper::vector<unsigned int> &index, const sofa::helper::vector<unsigned int> &inv_index)
{

    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    /// add the topological changes in the queue
    modifier->renumberPointsWarning(index, inv_index);
    // inform other objects that the triangles are going to be removed
    topology->propagateTopologicalChanges();
    // now renumber the points
    modifier->renumberPointsProcess(index, inv_index);

    assert(topology->getEdgeSetTopologyContainer()->checkTopology());
}

template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::addEdges(const sofa::helper::vector< Edge >& edges,
        const sofa::helper::vector< sofa::helper::vector< unsigned int > > & ancestors ,
        const sofa::helper::vector< sofa::helper::vector< double > >& baryCoefs)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    unsigned int nEdges=topology->getEdgeSetTopologyContainer()->getNumberOfEdges();

    /// actually add edges in the topology container
    modifier->addEdgesProcess(edges);

    sofa::helper::vector<unsigned int> edgesIndex;
    unsigned int i;
    for (i=0; i<edges.size(); ++i)
    {
        edgesIndex.push_back(nEdges+i);
    }
    // add topology event in the stack of topological events
    modifier->addEdgesWarning( edges.size(), edges,edgesIndex,ancestors,baryCoefs);
    // inform other objects that the edges are already added
    topology->propagateTopologicalChanges();

}
template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::swapEdges(const sofa::helper::vector< sofa::helper::vector< unsigned int > >& edgesPairs)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    modifier->swapEdgesProcess(edgesPairs);

    assert(topology->getEdgeSetTopologyContainer()->checkTopology());

}

template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::fuseEdges(const sofa::helper::vector< sofa::helper::vector< unsigned int > >& edgesPairs, const bool removeIsolatedPoints)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    modifier->fuseEdgesProcess(edgesPairs, removeIsolatedPoints);

    assert(topology->getEdgeSetTopologyContainer()->checkTopology());
}

template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::splitEdges( sofa::helper::vector<unsigned int> &indices,
        const sofa::helper::vector< sofa::helper::vector< double > >& baryCoefs,
        const bool removeIsolatedPoints)
{
    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyModifier< DataTypes >* modifier  = static_cast< EdgeSetTopologyModifier< DataTypes >* >(topology->getTopologyModifier());
    assert(modifier != 0);
    modifier->splitEdgesProcess(indices,baryCoefs,removeIsolatedPoints);

    assert(topology->getEdgeSetTopologyContainer()->checkTopology());

}


// Give the optimal vertex permutation according to the Reverse CuthillMckee algorithm (use BOOST GRAPH LIBRAIRY)
template<class DataTypes>
void EdgeSetTopologyAlgorithms< DataTypes >::resortCuthillMckee(sofa::helper::vector<int>& inverse_permutation)
{

    using namespace boost;
    using namespace std;
    typedef adjacency_list<vecS, vecS, undirectedS,
            property<vertex_color_t, default_color_type,
            property<vertex_degree_t,int> > > Graph;
    typedef graph_traits<Graph>::vertex_descriptor Vertex;
    typedef graph_traits<Graph>::vertices_size_type size_type;

    Graph G;

    EdgeSetTopology< DataTypes > *topology = dynamic_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());

    const sofa::helper::vector<Edge> &ea=container->getEdgeArray();
    if(ea.size()>0)
    {

        for (unsigned int k=0; k<ea.size(); ++k)
        {
            add_edge(ea[k][0], ea[k][1], G);
        }

        inverse_permutation.resize(num_vertices(G));

        //graph_traits<Graph>::vertex_iterator ui, ui_end;

        //property_map<Graph,vertex_degree_t>::type deg = get(vertex_degree, G);
        //for (boost::tie(ui, ui_end) = vertices(G); ui != ui_end; ++ui)
        //  deg[*ui] = degree(*ui, G);

        property_map<Graph, vertex_index_t>::type
        index_map = get(vertex_index, G);

        std::cout << "original bandwidth: " << bandwidth(G) << std::endl;

        std::vector<Vertex> inv_perm(num_vertices(G));
        std::vector<size_type> perm(num_vertices(G));

        //reverse cuthill_mckee_ordering
        cuthill_mckee_ordering(G, inv_perm.rbegin());

        //std::cout << "Reverse Cuthill-McKee ordering:" << endl;
        //std::cout << "  ";
        unsigned int ind_i = 0;
        for (std::vector<Vertex>::const_iterator i=inv_perm.begin();
                i != inv_perm.end(); ++i)
        {
            //std::cout << index_map[*i] << " ";
            inverse_permutation[ind_i]=index_map[*i];
            ind_i++;
        }
        //std::cout << endl;

        for (size_type c = 0; c != inv_perm.size(); ++c)
            perm[index_map[inv_perm[c]]] = c;
        std::cout << "  bandwidth: "
                << bandwidth(G, make_iterator_property_map(&perm[0], index_map, perm[0]))
                << std::endl;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////EdgeSetGeometryAlgorithms//////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
template< class DataTypes>
typename DataTypes::Real EdgeSetGeometryAlgorithms< DataTypes >::computeEdgeLength( const unsigned int i) const
{
    EdgeSetTopology< DataTypes > *topology = static_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    const Edge &e=container->getEdge(i);
    const VecCoord& p = *topology->getDOF()->getX();
    Real length=(p[e[0]]-p[e[1]]).norm();
    return length;
}
template< class DataTypes>
typename DataTypes::Real EdgeSetGeometryAlgorithms< DataTypes >::computeRestEdgeLength( const unsigned int i) const
{
    EdgeSetTopology< DataTypes > *topology = static_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    const Edge &e=container->getEdge(i);
    const VecCoord& p = *topology->getDOF()->getX0();
    Real length=(p[e[0]]-p[e[1]]).norm();
    return length;
}
template< class DataTypes>
typename DataTypes::Real EdgeSetGeometryAlgorithms< DataTypes >::computeRestSquareEdgeLength( const unsigned int i) const
{
    EdgeSetTopology< DataTypes > *topology = static_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    const Edge &e=container->getEdge(i);
    const VecCoord& p = *topology->getDOF()->getX0();
    Real length=(p[e[0]]-p[e[1]]).norm2();
    return length;
}
/// computes the edge length of all edges are store in the array interface
template<class DataTypes>
void EdgeSetGeometryAlgorithms<DataTypes>::computeEdgeLength( BasicArrayInterface<Real> &ai) const
{
    EdgeSetTopology< DataTypes > *topology = static_cast<EdgeSetTopology< DataTypes >* >(this->m_basicTopology);
    assert (topology != 0);
    EdgeSetTopologyContainer * container = static_cast< EdgeSetTopologyContainer* >(topology->getTopologyContainer());
    const sofa::helper::vector<Edge> &ea=container->getEdgeArray();
    const typename DataTypes::VecCoord& p = *topology->getDOF()->getX();
    unsigned int i;
    for (i=0; i<ea.size(); ++i)
    {
        const Edge &e=ea[i];
        ai[i]=(p[e[0]]-p[e[1]]).norm();
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////EdgeSetTopology//////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataTypes>
void EdgeSetTopology<DataTypes>::init()
{
    f_m_topologyContainer->beginEdit();
}
template<class DataTypes>
EdgeSetTopology<DataTypes>::EdgeSetTopology(MechanicalObject<DataTypes> *obj) : PointSetTopology<DataTypes>( obj,(PointSetTopology<DataTypes> *)0),
    f_m_topologyContainer(new DataPtr< EdgeSetTopologyContainer >(new EdgeSetTopologyContainer(), "Edge Container"))

{
    this->m_topologyContainer=f_m_topologyContainer->beginEdit();
    this->m_topologyContainer->setTopology(this);
    this->m_topologyModifier= new EdgeSetTopologyModifier<DataTypes>(this);
    this->m_topologyAlgorithms= new EdgeSetTopologyAlgorithms<DataTypes>(this);
    this->m_geometryAlgorithms= new EdgeSetGeometryAlgorithms<DataTypes>(this);
    this->addField(this->f_m_topologyContainer, "edgecontainer");
}


} // namespace topology

} // namespace component

} // namespace sofa

#endif // SOFA_COMPONENTS_EDGESETTOPOLOGY_INL
