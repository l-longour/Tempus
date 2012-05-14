#include <iostream>
#include <string>

#include <boost/graph/depth_first_search.hpp>

#include "tests.hh"

using namespace std;
using namespace Tempus;

#define DB_TEST_NAME "tempus_test_db"

void DbTest::testConnection()
{
    string db_options = g_db_options;

    // Connection to an non-existing database
    bool has_thrown = false;
    try
    {
	connection_ = new Db::Connection( db_options + " dbname=zorglub" );
    }
    catch ( std::runtime_error& )
    {
	has_thrown = true;
    }
    CPPUNIT_ASSERT_MESSAGE( "Must throw an exception when trying to connect to a non existing db", has_thrown );

    // Connection to an existing database
    has_thrown = false;
    try
    {
	connection_ = new Db::Connection( db_options + " dbname = " DB_TEST_NAME );
    }
    catch ( std::runtime_error& )
    {
	has_thrown = true;
    }
    CPPUNIT_ASSERT_MESSAGE( "Must not throw on an existing database, check that " DB_TEST_NAME " exists", !has_thrown );

    // Do not sigsegv ?
    delete connection_;
}

void DbTest::testQueries()
{
    string db_options = g_db_options;
    connection_ = new Db::Connection( db_options + " dbname = " DB_TEST_NAME );

    // test bad query
    bool has_thrown = false;
    try
    {
	connection_->exec( "SELZECT * PHROM zorglub" );
    }
    catch ( std::runtime_error& )
    {
	has_thrown = true;
    }
    CPPUNIT_ASSERT_MESSAGE( "Must throw an exception on bad SQL query", has_thrown );

    connection_->exec( "DROP TABLE IF EXISTS test_table" );
    connection_->exec( "CREATE TABLE test_table (id int, int_v int, bigint_v bigint, str_v varchar, time_v time)" );
    connection_->exec( "INSERT INTO test_table (id, int_v) VALUES ('1', '42')" );
    connection_->exec( "INSERT INTO test_table (id, int_v, bigint_v) VALUES ('2', '-42', '10000000000')" );
    connection_->exec( "INSERT INTO test_table (str_v) VALUES ('Hello world')" );
    connection_->exec( "INSERT INTO test_table (time_v) VALUES ('13:52:45')" );
    Db::Result res = connection_->exec( "SELECT * FROM test_table" );
    
    CPPUNIT_ASSERT_EQUAL( (size_t)4, res.size() );
    CPPUNIT_ASSERT_EQUAL( (size_t)5, res.columns() );
    CPPUNIT_ASSERT_EQUAL( (int)1, res[0][0].as<int>() );
    CPPUNIT_ASSERT_EQUAL( (int)42, res[0][1].as<int>() );
    CPPUNIT_ASSERT( res[0][2].is_null() );

    CPPUNIT_ASSERT_EQUAL( (int)-42, res[1][1].as<int>());
    CPPUNIT_ASSERT_EQUAL( 10000000000ULL, res[1][2].as<unsigned long long>() );

    CPPUNIT_ASSERT_EQUAL( string("Hello world"), res[2][3].as<string>() );

    Tempus::Time t = res[3][4].as<Tempus::Time>();
    CPPUNIT_ASSERT_EQUAL( (long)(13 * 3600 + 52 * 60 + 45), t.n_secs );

    delete connection_;
}

void PgImporterTest::setUp()
{
    string db_options = g_db_options;
    importer_ = new PQImporter( db_options + " dbname = " DB_TEST_NAME );
}

void PgImporterTest::tearDown() 
{
    delete importer_;
}

void PgImporterTest::testConsistency()
{
    importer_->import_constants( graph_ );
    importer_->import_graph( graph_ );

    // get the number of vertices in the graph
    Db::Result res = importer_->query( "SELECT COUNT(*) FROM tempus.road_node" );
    CPPUNIT_ASSERT( res.size() == 1 );
    long n_road_vertices = res[0][0].as<long>();
    res = importer_->query( "SELECT COUNT(*) FROM tempus.road_section" );
    CPPUNIT_ASSERT( res.size() == 1 );
    long n_road_edges = res[0][0].as<long>();
    cout << "n_road_vertices = " << n_road_vertices << " n_road_edges = " << n_road_edges << endl;
    CPPUNIT_ASSERT( n_road_vertices = boost::num_vertices( graph_.road ) );
    CPPUNIT_ASSERT( n_road_edges = boost::num_edges( graph_.road ) );
    
    // number of PT networks
    res = importer_->query( "SELECT COUNT(*) FROM tempus.pt_network" );
    long n_networks = res[0][0].as<long>();
	
    CPPUNIT_ASSERT_EQUAL( (size_t)n_networks, graph_.public_transports.size() );
    CPPUNIT_ASSERT_EQUAL( (size_t)n_networks, graph_.network_map.size() );

    Multimodal::Graph::PublicTransportGraphList::iterator it;
    for ( it = graph_.public_transports.begin(); it != graph_.public_transports.end(); it++ )
    {
	db_id_t id = it->first;
	PublicTransport::Graph& pt_graph = it->second;

	res = importer_->query( "SELECT COUNT(*) FROM tempus.pt_stop" );
	CPPUNIT_ASSERT( res.size() == 1 );
	long n_pt_vertices = res[0][0].as<long>();
	res = importer_->query( "SELECT COUNT(*) FROM tempus.pt_section" );
	CPPUNIT_ASSERT( res.size() == 1 );
	long n_pt_edges = res[0][0].as<long>();
	CPPUNIT_ASSERT( n_pt_vertices = boost::num_vertices( pt_graph ) );
	CPPUNIT_ASSERT( n_pt_edges = boost::num_edges( pt_graph ) );
    }
}

void PgImporterTest::testMultimodal()
{
    importer_->import_constants( graph_ );
    importer_->import_graph( graph_ );

    size_t nv = 0;
    size_t n_road_vertices = 0;
    size_t n_pt_vertices = 0;
    size_t n_pois = 0;
    Multimodal::VertexIterator vi, vi_end;
    for ( boost::tie(vi, vi_end) = vertices( graph_ ); vi != vi_end; vi++ )
    {
	nv++;
	if ( vi->type == Multimodal::Vertex::Road )
	    n_road_vertices++;
	else if ( vi->type == Multimodal::Vertex::PublicTransport )
	    n_pt_vertices++;
	else
	    n_pois ++;
    }

    const PublicTransport::Graph& pt_graph = graph_.public_transports.begin()->second;
    cout << "nv = " << nv << endl;
    cout << "n_road_vertices = " << n_road_vertices << " num_vertices(road) = " << num_vertices( graph_.road ) << endl;
    cout << "n_pt_vertices = " << n_pt_vertices << " num_vertices(pt) = " << num_vertices( pt_graph ) << endl;
    cout << "n_pois = " << n_pois << " pois.size() = " << graph_.pois.size() << endl;
    cout << "num_vertices = " << num_vertices( graph_ ) << endl;
    CPPUNIT_ASSERT_EQUAL( nv, num_vertices( graph_ ) );

    for ( boost::tie(vi, vi_end) = vertices( graph_ ); vi != vi_end; vi++ )
    {
    	Multimodal::OutEdgeIterator oei, oei_end;
    	boost::tie( oei, oei_end ) = out_edges( *vi, graph_ );
	size_t out_deg = 0;
    	for ( ; oei != oei_end; oei++ )
    	{
    	    out_deg++;
    	}
	size_t out_deg2 = out_degree( *vi, graph_ );
	CPPUNIT_ASSERT_EQUAL( out_deg, out_deg2 );
    }
    Multimodal::EdgeIterator ei, ei_end;
    size_t ne = 0;
    size_t n_road2road = 0;
    size_t n_road2transport = 0;
    size_t n_transport2road = 0;
    size_t n_transport2transport = 0;
    size_t n_road2poi = 0;
    size_t n_poi2road = 0;
    
    bool b;
    for ( boost::tie( ei, ei_end ) = edges( graph_ ); ( b = ei != ei_end ); ei++ )
    {
	ne++;
	switch ( ei->connection_type() )
	{
	case Multimodal::Edge::Road2Road:
	    n_road2road++;
	    break;
	case Multimodal::Edge::Road2Transport:
	    n_road2transport++;
	    break;
	case Multimodal::Edge::Transport2Road:
	    n_transport2road++;
	    break;
	case Multimodal::Edge::Transport2Transport:
	    n_transport2transport++;
	    break;
	case Multimodal::Edge::Road2Poi:
	    n_road2poi++;
	    break;
	case Multimodal::Edge::Poi2Road:
	    n_poi2road++;
	    break;
	}
    }

    size_t n_stops = 0;
    Road::EdgeIterator pei, pei_end;
    for ( boost::tie( pei, pei_end ) = edges( graph_.road ); pei != pei_end; pei++ )
    {
	n_stops += graph_.road[ *pei ].stops.size();
    }

    cout << "ne = " << ne << endl;
    cout << "n_road2road = " << n_road2road << " num_edges(road) = " << num_edges( graph_.road ) << endl;
    cout << "n_road2transport = " << n_road2transport << endl;
    cout << "n_transport2road = " << n_transport2road << endl;
    cout << "n_road2poi = " << n_road2poi << endl;
    cout << "n_poi2road = " << n_poi2road << " pois.size = " << graph_.pois.size() << endl;
    cout << "n_transport2transport = " << n_transport2transport << " num_edges(pt) = " << num_edges( pt_graph ) << endl;
    size_t sum = n_road2road + n_road2transport + n_transport2road + n_transport2transport + n_poi2road + n_road2poi;
    cout << "sum = " << sum << endl;
    cout << "num_edges = " << num_edges( graph_ ) << endl;
    CPPUNIT_ASSERT_EQUAL( sum, num_edges( graph_ ) );

    // test graph traversal
    {
	std::map<Multimodal::Vertex, boost::default_color_type> colors;
	boost::depth_first_search( graph_,
				   boost::dfs_visitor<boost::null_visitor>(),
				   boost::make_assoc_property_map( colors )
				   );
    }
}

