
/**
 * Copyright (C) 2019 bolthur project.
 *
 * This file is part of bolthur/serial-communicator.
 *
 * bolthur/serial-communicator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bolthur/serial-communicator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bolthur/serial-communicator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#include "serial.h"
#include "kernel.h"

/**
 * @brief serial handle
 */
static serial_handle_t handle = 0;

/**
 * @brief file buffer
 */
static uint8_t* file_buffer = NULL;

/**
 * @brief file length
 */
static uint32_t file_length = 0;

/**
 * @brief Cleanup method on exit
 */
void cleanup( void ) {
  // close serial handle
  serial_close( handle );

  // free file buffer if set
  if ( NULL != file_buffer ) {
    free( file_buffer );
  }
}

/**
 * @brief Main entry point for serial communicator
 *
 * @param argc argument count
 * @param argv argument array
 * @return int
 */
int main( int argc, char** argv ) {
  // variables
  char *device;
  char *file;
  bool finished = false;
  ssize_t written;
  int32_t breaks_received;
  uint8_t buffer;
  ssize_t bytes_received;

  // initial print of name and version
  printf( "%s %s\r\n", PACKAGE_NAME, PACKAGE_VERSION );

  // handle not enough parameter
  if ( 3 != argc ) {
    // print example and usage
    printf( "Example: %s /dev/ttyUSB0 kernel.img\r\n", argv[ 0 ] );
    printf( "Usage: %s <device> <file>\r\n", argv[ 0 ] );

    // exit with error
    exit( EXIT_FAILURE );
  }

  // register exit function
  atexit( cleanup );

  // save device and file
  device = argv[ 1 ];
  file = argv[ 2 ];

  // load file to transfer
  printf( "Try to load file \"%s\" for transfer\r\n", file );
  kernel_load( file, &file_buffer, &file_length );

  if ( NULL == file_buffer ) {
    exit( EXIT_FAILURE );
  }

  while( ! finished ) {
    // open serial handle
    printf( "Try to open device \"%s\"\r\n", device );
    handle = serial_open( device );

    // error handling
    if ( INVALID_HANDLE == handle ) {
      // check for errors to skip
      if ( ENOENT == errno || ENODEV == errno || EACCES == errno ) {
        printf( "\rWaiting for %s to be ready!\r", device );
        sleep( 1 );
        continue;
      }

      // handle exit
      exit( EXIT_FAILURE );
    }

    // progress output
    printf( "Listening on device %s\r\n", device );
    printf( "Waiting for breaks via device!\r\n" );

    // amount of breaks from serial
    breaks_received = 0;
    while ( 3 > breaks_received ) {
      // wait for 3 breaks
      bytes_received = serial_read( handle, &buffer, 1 );

      // skip when no bytes have been transmitted
      if ( 0 >= bytes_received ) {
        continue;
      }

      // handle debug break
      if ( '\x03' == buffer ) {
        breaks_received++;
        continue;
      }

      // else case we received something different
      breaks_received = 0;
      printf( "%c", buffer );
    }

    // Send loaded file from buffer
    printf( "Sending loaded file via serial device!\r\n" );

    // send kernel size in bytes
    written = serial_write( handle, &file_length, 4 );
    printf( "Written bytes for kernel size: %ld\r\n", written );

    // send kernel
    written = serial_write( handle, file_buffer, file_length );
    printf( "Written bytes for kernel: %ld\r\n", written );
    printf( "buffer length: %d\r\n", file_length );

    // print serial device output
    while ( true ) {
      bytes_received = serial_read( handle, &buffer, 1 );

      // skip when no bytes have been transmitted
      if ( 0 >= bytes_received ) {
        continue;
      }

      // print
      printf( "%c", buffer );
    }
  }

  exit( EXIT_SUCCESS );
}
