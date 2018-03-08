#define WINDOWS

#ifdef WINDOWS
    #include<windows.h>
#else
    #include<dlfcn.h>
#endif

#include<ffi.h>
#include<stdint.h>
#include"xmlbare/parser.h"
#include"xmlbare/string_tree.h"

#define newType( structName, newName ) structName *newName = ( structName * ) malloc( sizeof( structName ) ); memset( (char *) newName, 0, sizeof( structName ) );

#define TYP_DLL_FUNC 1

typedef void (*funcPtr)(void);

typedef struct hInfoBase {
    char rawtype;
    funcPtr func;
} hInfoBase;

typedef struct hInfoDllFunc {
    char rawtype;
    funcPtr func;
    funcPtr rawfunc;
    ffi_cif ffiOb;
    ffi_type **args;
    ffi_type *retType;
} hInfoDllFunc;

typedef void (*nodeHandler)( hInfoBase *handlerInfo, node *node );

static void handlePrintf( hInfoBase *handlerInfo, node *node );
static void handleSet( hInfoBase *handlerInfo, node *node );

#ifdef WINDOWS
static void setupDllFunc( string_tree *handlers, HINSTANCE dllHandle, char *func, char *argSpec, char *retStr );
#else
static void setupDllFunc( string_tree *handlers, void *dllHandle, char *func, char *argSpec, char *retStr );
#endif

static void handleDllFunc( hInfoBase *handlerInfo, node *node );
static void handleDllFuncElip( hInfoBase *handlerInfo, node *node );

typedef struct var {
    char type;
} var;
typedef struct ptrvar {
    char type;
    void *ptr;
} ptrvar;
string_tree *vartree;

// memory for static variables; can dump on cleanup
char *varmem;
int varpos;
int varmax;

// memory for malloc'ed data; each must be freed
ptrvar **ptmem;
int ptpos;
int ptmax;

static void varTree__init() {
    vartree = string_tree_c__new();
    varmem = (char *) malloc( 30000 );
    varpos = 0;
    varmax = 30000;
    
    ptmem = (ptrvar **) malloc( sizeof( ptrvar ) * 2000 );
    ptmax = 2000;
    ptpos = 0;
}

static void varTree__destroy() {
    free( varmem );
    string_tree_c__destroy( vartree );
}

#define VT_ch 1 // char
#define VT_u1 2 // 1 byte unsigned int
#define VT_u2 5 // 2 byte unsigned int
#define VT_pt 3 // pointer
#define VT_ap 4 // allocated pointer
static var *getVar( char *name, int namelen ) {
    var *varOb = string_tree_c__get_wlen( vartree, name, namelen, NULL );
    return varOb;
}

static void *setVar_base( char *name, int namelen, short size, char type ) {
    var *varOb = getVar( name, namelen );
    char *dataLoc;
    if( !varOb ) {
        varOb = (var *) &varmem[ varpos ];
        varOb->type = type;
        dataLoc = ( char * ) &varmem[ varpos + 1 ];
        varpos += size;
        string_tree_c__store_wlen( vartree, name, namelen, (void *) varOb );
    }
    else dataLoc = ( char * ) ( varOb + 1 );
    return dataLoc;
}

static void setVar_ch( char *name, int namelen, char    val ) {
    char*     dest = ( char*     ) setVar_base( name, namelen, sizeof( char     ), VT_ch ); *dest = val; }
static void setVar_u1( char *name, int namelen, uint8_t val ) {
    uint8_t*  dest = ( uint8_t*  ) setVar_base( name, namelen, sizeof( uint8_t  ), VT_u1 ); *dest = val; }
static void setVar_u2( char *name, int namelen, uint8_t val ) {
    uint16_t* dest = ( uint16_t* ) setVar_base( name, namelen, sizeof( uint16_t ), VT_u2 ); *dest = val; }
static void setVar_pt( char *name, int namelen, void *  val ) {
    void**    dest = ( void**    ) setVar_base( name, namelen, sizeof( void *   ), VT_pt ); *dest = val; }

static void setVar_ap( char *name, int namelen, void * val ) {
    ptrvar *varOb = (ptrvar *) getVar( name, namelen );
    if( !varOb ) {
        varOb = (ptrvar *) &ptmem[ ptpos ];
        varOb->type = VT_ap;
        varOb->ptr = val;
        ptpos += 1 + sizeof( void * );
        string_tree_c__store_wlen( vartree, name, namelen, (void *) varOb );
    }
    else {
        free( varOb->ptr );
        varOb->ptr = val;
    }
}


int main() {// int argc, char *argv 
    string_tree *handlers = string_tree_c__new();
    varTree__init();
    
    //newType( hInfoBase, printfInfo );
    //printfInfo->func = ( funcPtr ) handlePrintf;
    //printfInfo->rawtype = 0;
    //string_tree_c__store( handlers, "printf", (void *) printfInfo );
    
    newType( hInfoBase, setInfo );
    setInfo->func = ( funcPtr ) handleSet;
    setInfo->rawtype = 0;
    string_tree_c__store( handlers, "set", (void *) setInfo );
    
    struct parserc *parser = parserc__new();
    if( !parser ) {
        printf("Could not create parser\n");
        exit(1);
    }
    node *root = parserc__file( parser, "test.xml" );
    if( root ) {
        arr *dlls = nodec__getnodes( root, "dll" );
        if( dlls ) {
            int dllCount = dlls->count;
            for( int i=0;i<dllCount;i++ ) {
                node *dll = ( node * ) dlls->items[ i ];
                char *dllName = nodec__getattval( dll, "name");
                if( !dllName ) continue;
                
                #ifdef WINDOWS
                    HINSTANCE dllHandle = LoadLibrary( dllName );
                #else
                    void *dllHandle = dlopen( dllName, RTLD_LOCAL | RTLD_LAZY );
                #endif
                if( !dllHandle ) {
                    printf("Could not load library %s\n", dllName );
                    continue;
                }
                
                arr *is = nodec__getnodes( dll, "i" );
                if( !is ) continue;
                int count = is->count;
                for( int j=0;j<count;j++ ) {
                    node *import = ( node * ) is->items[ j ];
                    char *func = nodec__getattval( import, "f" );
                    char *argStr = nodec__getattval( import, "a" );
                    char *retStr = nodec__getattval( import, "r" );
                    
                    setupDllFunc( handlers, dllHandle, func, argStr, retStr );
                    //printf("Import - func=%s, args=%s\n", func, argStr );
                }
                arrc__destroy( is );
            }
            arrc__destroy( dlls );
        }
        
        node *xml = nodec__getnode( root, "xml" );
        if( xml ) {
            node *curnode = xml->firstchild;
            while( curnode ) {
                if( curnode->rawtype ) {
                    curnode = curnode->next;
                    continue;
                }
                char *name = curnode->name;
                int nameLen = curnode->namelen;
                hInfoBase *infoBase = ( hInfoBase * ) string_tree_c__get_wlen( handlers, name, nameLen, NULL );
                if( infoBase ) {
                    nodeHandler func = ( nodeHandler ) infoBase->func;
                    func( infoBase, curnode );
                }
                else {
                    printf("skipping %.*s ; no handler\n", nameLen, name );
                }
                curnode = curnode->next;
            }
        }
    }
    parserc__destroy( parser );
    string_tree_c__destroy( handlers );
            
    return 0;
}

ffi_type *charsToFFI( char *let ) {
    if( !strncmp( let, "pt", 2 ) ) return &ffi_type_pointer;
    if( !strncmp( let, "vo", 2 ) ) return &ffi_type_void;
}

#ifdef WINDOWS
static void setupDllFunc( string_tree *handlers, HINSTANCE dllHandle, char *func, char *argSpec, char *retStr ) {
#else
static void setupDllFunc( string_tree *handlers, void *dllHandle, char *func, char *argSpec, char *retStr ) {
#endif
    newType( hInfoDllFunc, info );
    info->rawtype = TYP_DLL_FUNC;
    
    #ifdef WINDOWS
    funcPtr rawFunc = ( funcPtr ) GetProcAddress( dllHandle, func );
    #else
    funcPtr rawFunc = ( funcPtr ) dlsym( dllHandle, func );
    #endif
    
    if( !rawFunc ) { printf("Could not get func %s from dll\n", func ); free( info ); return; }
    
    info->rawfunc = rawFunc;
    
    int argLen = strlen( argSpec );
    int numArgs = argLen / 2;
    info->args = malloc( sizeof( ffi_type * ) * numArgs );
    
    
    ffi_type *retType = charsToFFI( retStr );
    
    short isElip = 0;
    if( numArgs == 2 && argSpec[ 2 ] == '.' && argSpec[ 3 ] == '.' ) {
        isElip = 1;
        numArgs--;
        info->func = ( funcPtr ) handleDllFuncElip;
        info->retType = retType;
        printf("Setting up var dll func %s\n", func );
        printf("  Fixed Arg Count: %i\n", numArgs );
    }
    else {
        printf("Setting up dll func %s\n", func );
        printf("  Arg Count: %i\n", numArgs );
        info->func = ( funcPtr ) handleDllFunc;
    }
    
    
    for( int i=0;i<numArgs;i++ ) {
        info->args[i] = charsToFFI( &argSpec[ i*2 ] );
        if( !info->args[i] ) { printf("  Failed to set type of argument %i\n", i ); free(info->args); free(info); return; }
    }
    
    
    if( !retType ) { printf("  Failed to set a return type\n"); free(info->args); free(info); return; }
    
    if( !isElip ) {
        ffi_status res = ffi_prep_cif( &info->ffiOb, FFI_DEFAULT_ABI, numArgs, retType, info->args );
        if( res != FFI_OK ) { printf("  Failed to prepare ffi cif\n"); free(info->args); free(info); return; }
    }
    
    string_tree_c__store( handlers, func, (void *) info );
}

static void handleDllFunc( hInfoBase *handlerInfo, node *node ) {
    hInfoDllFunc *info = ( hInfoDllFunc * ) handlerInfo;
    
    void *resultPtr;
    void *values[2];
        
    for( int i=1;i<10;i++ ) {
        char attname[ 4 ];
        //sprintf( attname, "p%i", i );
        //printf("Attempting to fetch %s\n", attname );
        //att *paramAtt = nodec__getatt( node, attname );
        att *paramAtt = nodec__getatt_bypos( node, i-1 );
        printf("Attempting to fetch att pos %i\n", i );
        if( !paramAtt ) break;
        var *info = getVar( paramAtt->value, paramAtt->vallen );
        if( !info ) {
            printf("Could not fetch var %.*s\n", paramAtt->vallen, paramAtt->value );
            return;
        }
        
        if( info->type == VT_ap ) {
            ptrvar *info2 = ( ptrvar * ) info;
            values[ i-1 ] = (void *) &info2->ptr;
        }
        else {
            printf("Unknown var type for param %i(%.*s) - type=%i\n", i, paramAtt->vallen, paramAtt->value, info->type );
            return;
        }
    }
    
    ffi_call( &info->ffiOb, info->rawfunc, &resultPtr, values );
}

static void handleDllFuncElip( hInfoBase *handlerInfo, node *node ) {
    hInfoDllFunc *info = ( hInfoDllFunc * ) handlerInfo;
    
    void *resultPtr;
    //void *values[2];
    
    int numatts = node->numatt;
    printf("Att count for elip call: %i\n", numatts );
    void **values = (void **) malloc( sizeof( void * ) * ( numatts ) );
    //values[ numatts ] = 0x00;
    char *toFree = (char *) malloc( numatts );
    char someFree = 0;
    memset( toFree, 0, numatts );
    
    ffi_type **args = info->args;
    ffi_type *args1 = args[0];
    free( args );
    args = (ffi_type **) malloc( sizeof( ffi_type * ) * numatts );
    args[0] = args1;
    info->args = args;
    
    for( int i=0; i < numatts ; i++ ) {
        printf("Attempting to fetch att pos %i\n", i );
        att *paramAtt = nodec__getatt_bypos( node, i );
        if( !paramAtt ) {
            printf("dyncall - could not fetch att pos %i\n", i );
        } // problem
        
        if( paramAtt->colon ) {
            var *info = getVar( paramAtt->value, paramAtt->vallen );
            if( !info ) {
                printf("Could not fetch var %.*s\n", paramAtt->vallen, paramAtt->value );
                return;
            }
            else {
                ptrvar *info2 = ( ptrvar * ) info;
                printf("Fetched variable named %.*s for pos %i\n", paramAtt->vallen, paramAtt->value, i );
                printf("Value of variable: %s\n", info2->ptr );
            }
            
            if( info->type == VT_ap ) {
                ptrvar *info2 = ( ptrvar * ) info;
                values[ i ] = (void *) &info2->ptr;
                if( i ) args[ i ] = &ffi_type_pointer;
            }
            else {
                printf("Unknown var type for param %i(%.*s) - type=%i\n", i, paramAtt->vallen, paramAtt->value, info->type );
                return;
            }
        }
        else { // string
            char **strPtr = ( char ** ) malloc( sizeof( char ** ) );
            *strPtr = (char *) malloc( paramAtt->vallen + 1 );
            printf("Value of att %i: [%.*s]\n", i, paramAtt->vallen, paramAtt->value );
            strncpy( *strPtr, paramAtt->value, paramAtt->vallen );
            (*strPtr)[ paramAtt->vallen ] = 0x00;
            values[ i ] = strPtr;//paramAtt->value;
            toFree[ i ] = 1;
            someFree = 1;
            if( i ) args[ i ] = &ffi_type_pointer;
        }
    }
    
    ffi_status res = ffi_prep_cif_var( &info->ffiOb, FFI_DEFAULT_ABI, 1, numatts, info->retType, info->args );
    if( res != FFI_OK ) { printf("  Failed to dyncall function\n"); return; }
    
    ffi_call( &info->ffiOb, info->rawfunc, &resultPtr, values );
    
    if( someFree )
        for( int i=0; i<numatts; i++ ) {
            char freeType = toFree[i];
            if( freeType == 1 ) {
                char **ptr = (char **) values[ i ];
                free( *ptr );
                free( ptr );
            }
        }
}

static void handlePrintf( hInfoBase *handlerInfo, node *node ) {
    //att *att = nodec__getatt( node, "val" );
    //char *val = attc__getval( att );
    char *form = nodec__getattval_bypos( node, 0 );
    
    att *p1Att = nodec__getatt_bypos( node, 1 );
    if( p1Att ) {
        var *info = getVar( p1Att->value, p1Att->vallen );
        if( info->type == VT_ap ) {
            ptrvar *info2 = ( ptrvar * ) info;
            printf( form, ( void * ) info2->ptr );
        }
        
        //printf( form );
    }
    else {
        printf( form );
    }
    printf("\n");
}

static void handleSet( hInfoBase *handlerInfo, node *node ) {
    char *type = nodec__getattval( node, "type" );
    char *val = nodec__getattval_bypos( node, 1 );
    if( !strncmp( type, "str", 3 ) ) {
        char *strCopy = strdup( val );
        att *nameAtt = nodec__getatt_bypos( node, 0 );
        printf("Setting %.*s to '%s'\n", nameAtt->vallen, nameAtt->value, strCopy );
        setVar_ap( nameAtt->value, nameAtt->vallen, strCopy );
    }
}