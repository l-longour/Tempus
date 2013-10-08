#!/usr/bin/env python

# for PostgreSQL connection
import psycopg2
import argparse
import random
import time

from tempus_request import *

import matplotlib.pyplot as plt

dbstring="dbname=tempus_test_db"
wpsurl="http://127.0.0.1/wps"

# test command line arguments
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Tempus batch')
    parser.add_argument('-d', '--dbstring', required=False,
            help='The database connection string')
    parser.add_argument('-w', '--wpsurl', required=False,
            help='The WPS server URL')

    args = parser.parse_args()

    if args.dbstring:
        dbstring = args.dbstring
    if args.wpsurl:
        wpsurl = args.wpsurl

    tempus = TempusRequest( wpsurl )

    # connection to db
    conn = psycopg2.connect( dbstring )
    cur = conn.cursor()

    # get every road node
    cur.execute( "SELECT st_x(geom) as x, st_y(geom) as y FROM tempus.road_node" )
    points = []
    for pt in cur:
        points.append( Point(float(pt[0]), float(pt[1])) )

    random.seed()

    times = []
    distances = []
    total_time = 0
    total_rtime = 0
    for i in range(0,100):

        # pick two points at random and ask for an itinerary
        n = len(points)
        dep = points[ random.randrange(n) ]
        arr = points[ random.randrange(n) ]

        # distance computation
        dist = (dep.x-arr.x)**2 + (dep.y-arr.y)**2

        start_time = time.time()

        # the actual request
        tempus.request( plugin_name = "sample_road_plugin",
                        plugin_options = { 'prepare_result' : 0 },
                        origin = dep,
                        steps = [Step(destination = arr)] )

        # time spend by the algorithm
        t = float(tempus.metrics['time_s'])
        total_time = total_time + t

        # time spend by all the WPS communication
        r = time.time() - start_time
        total_rtime += r

        print "i=%d, t=%f, r=%f" % (i, t, r)

        times.append(t)
        distances.append(dist)

    print total_time, total_rtime

    plt.clf()
    plt.xlabel( "Time(s)" )
    plt.ylabel( "Distance" )
    
    plt.plot( times, distances, 'ro' )
    plt.show()

