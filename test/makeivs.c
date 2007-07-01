#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SWAP(x,y) { unsigned char tmp = x; x = y; y = tmp; }
#define IVS2_MAGIC "\xAE\x78\xD1\xFF"
#define IVS2_EXTENSION		"ivs"
#define IVS2_VERSION             1

#define IVS2_BSSID	0x01
#define IVS2_ESSID	0x02
#define IVS2_WPA	0x04
#define IVS2_XOR	0x08
#define IVS2_PTW        0x10

struct ivs2_pkthdr
{
    unsigned short flags;
    unsigned short len;
};

struct ivs2_filehdr
{
    unsigned short version;
};

int main( int argc, char *argv[] )
{
    int i, j, k, n, count=100000, length=16, keylen, zero=0, startiv=0, iv=0;
    FILE *f_ivs_out;
    unsigned char K[16];
    unsigned char S[256];
    unsigned char buffer[64], *s;
    struct ivs2_pkthdr ivs2;
    struct ivs2_filehdr fivs2;
    unsigned long long size;

    i = 0;

    if(  argc < 3 || argc > 6 )
    {
        printf( "usage: %s <ivs file> <wep key> [<count> [<length> [<startiv>]]]\n", argv[0]);
        printf( "example: %s test.ivs ABCDEF01234567890123456789 50000 16 0\n", argv[0] );
        return( 1 );
    }

    if( argc >= 4 )
        count = atoi(argv[3]);

    if( argc >= 5 )
        length = atoi(argv[4]);

    if( argc >= 6 )
        startiv = atoi(argv[5]);

    if(count < 0 || count > 0xFFFFFF)
    {
        fprintf(stderr, "Invalid number of IVs. (%d)\n", count);
        return( 1 );
    }
    if(count == 0)
        count = 100000; //default 100.000 ivs

    if(length < 0 || length > 0xFFFF)
    {
        fprintf(stderr, "Invalid number of keystreambytes. (%d)\n", length);
        return( 1 );
    }
    if(length == 0)
        length = 16; //default 16 keystreambytes

    s = (unsigned char *) argv[2];

    buffer[0] = s[0];
    buffer[1] = s[1];
    buffer[2] = '\0';

    while( sscanf( (char*) buffer, "%x", &n ) == 1 )
    {
        if( n < 0 || n > 255 )
        {
            fprintf( stderr, "Invalid wep key.\n" );
            return( 1 );
        }

        K[3 + i++] = n;

        if( i >= 32 ) break;

        s += 2;

        if( s[0] == ':' || s[0] == '-' )
            s++;

        if( s[0] == '\0' || s[1] == '\0' )
            break;

        buffer[0] = s[0];
        buffer[1] = s[1];
    }

    if( i != 5 && i != 13 && i != 29 )
    {
        fprintf( stderr, "Invalid wep key.\n" );
        return( 1 );
    }

    keylen = i+3;

    size = (long long)strlen(IVS2_MAGIC) + (long long)sizeof(struct ivs2_filehdr) + (long long)count *
           (long long)sizeof(struct ivs2_pkthdr) + (long long)count * (long long)length;

    printf("Creating %d IVs with %d bytes of keystream each.\n", count, length);
    printf("Estimated filesize: ");
    if(size > 1024*1024*1024)   //over 1 GB
        printf("%.2f GB\n", ((double)size/(1024.0*1024.0*1024.0)));
    else if (size > 1024*1024)  //over 1 MB
        printf("%.2f MB\n", ((double)size/(1024.0*1024.0)));
    else if (size > 1024)       //over 1 KB
        printf("%.2f KB\n", ((double)size/1024.0));
    else                        //under 1 KB
        printf("%.2f Byte\n", (double)size);

    if( ( f_ivs_out = fopen( argv[1], "wb+" ) ) == NULL )
    {
        perror( "fopen" );
        return( 1 );
    }

    fprintf( f_ivs_out, IVS2_MAGIC );

    memset(&fivs2, '\x00', sizeof(struct ivs2_filehdr));
    fivs2.version = IVS2_VERSION;

    /* write file header */
    if( fwrite( &fivs2, 1, sizeof(struct ivs2_filehdr), f_ivs_out )
        != (size_t) sizeof(struct ivs2_filehdr) )
    {
        perror( "fwrite(IV file header) failed" );
        return( 1 );
    }

    memset(&ivs2, '\x00', sizeof(struct ivs2_pkthdr));
    ivs2.flags |= IVS2_BSSID;
    ivs2.len += 6;

    /* write header */
    if( fwrite( &ivs2, 1, sizeof(struct ivs2_pkthdr), f_ivs_out )
        != (size_t) sizeof(struct ivs2_pkthdr) )
    {
        perror( "fwrite(IV header) failed" );
        return( 1 );
    }

    /* write BSSID */
    if( fwrite( "\x01\x02\x03\x04\x05\x06", 1, 6, f_ivs_out )
        != (size_t) 6 )
    {
        perror( "fwrite(IV bssid) failed" );
        return( 1 );
    }
    printf("Using fake BSSID 01:02:03:04:05:06\n");

    for( n = 0; n < count; n++ )
    {
        iv = (n + startiv) & 0xFFFFFF;
        K[2] = ( iv >> 16 ) & 0xFF;
        K[1] = ( iv >>  8 ) & 0xFF;
        K[0] = ( iv       ) & 0xFF;

        for( i = 0; i < 256; i++ )
            S[i] = i;

        for( i = j = 0; i < 256; i++ )
        {
            j = ( j + S[i] + K[i % keylen] ) & 0xFF;
            SWAP( S[i], S[j] );
        }

        memset(&ivs2, '\x00', sizeof(struct ivs2_pkthdr));
        ivs2.flags = 0;
        ivs2.len = 0;

        ivs2.flags |= IVS2_XOR;
        ivs2.len += length+4;

        if( fwrite( &ivs2, 1, sizeof(struct ivs2_pkthdr), f_ivs_out )
            != (size_t) sizeof(struct ivs2_pkthdr) )
        {
            perror( "fwrite(IV header) failed" );
            return( 1 );
        }

        if( fwrite( K, 1, 3, f_ivs_out ) != (size_t) 3 )
        {
            perror( "fwrite(IV iv) failed" );
            return( 1 );
        }
        if( fwrite( &zero, 1, 1, f_ivs_out ) != (size_t) 1 )
        {
            perror( "fwrite(IV idx) failed" );
            return( 1 );
        }
        ivs2.len -= 4;

        i = j = 0;
        for( k=0; k < length; k++ )
        {
            i = (i+1) & 0xFF; j = ( j + S[i] ) & 0xFF; SWAP(S[i], S[j]);
            fprintf( f_ivs_out, "%c", S[(S[i] + S[j]) & 0xFF] );
        }
    }

    fclose( f_ivs_out );
    printf( "Done.\n" );
    return( 0 );
}

