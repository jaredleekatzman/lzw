/* StringTable data structure for the Lempel-Ziv-Welch algorithm
 * by Jared Katzman (9/16/14)
 *
 *  Stores code in a Trie based on (PREFIX, K) and hashes the 
 *  nodes based on CODE to a dynamic-length array
 *
 *  stringTable can store at most 2^MAXBITS codes
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <ctype.h>
#include "StringTable.h"
#include "code.h"

struct node {
    unsigned char K;
    int Prefix, Code, Count, nChild;
    Node *Child;
};

static Node *stringTable;                               // Dynamical-Size Array (CODE, NODE)
static Node stringTrie;                                 // Trie of (PREFIX,CODE,K)
static int nCodes = 0;                                  // # of codes in the stringTable
static int tSize = 2;                                   // 2^tSize - current table size
int MAXBITS = 12;                                       // Table Capacity: 2^MAXBITS codes
int PRUNE = false;                                      // PRUNE flag
int ESCAPE = false;                                     // ESCAPE flag

// Debug - Stage MODE flag
static int MODE = 3;

void initializeTable (int e_flag) {
    
    tSize = e_flag ? 2 : CHAR_BIT+1;                    // Mininum size to hold initial codes

    stringTable = calloc (sizeof(Node), (1 << tSize));  // 2 ^ tSize
    stringTrie = createNode ('\0', 0, EMPTY);           // initialize EMPTY
    nCodes = 0;
    stringTable[nCodes++] = stringTrie;
    stringTable[nCodes++] = createNode('\0', 0, ESC);   // ESC
    stringTable[nCodes++] = createNode('\0',0, END);    // END

    if (!e_flag) {                                      // -e Flag
        for (int i = 0; i < (1 << CHAR_BIT); i++) {     // 0 - 255 ASCII
            unsigned char k = i;
            insert (EMPTY, k, false);
        }
    }
}

Node createNode (unsigned char k, int prefix, int code) {
    Node t = (Node) malloc(sizeof(struct node));
    if (t == NULL)
        return NULL;
    t->K = k;
    t->Prefix = prefix;
    t->Code = code;
    t->Count = 1;
    t->Child = NULL;
    t->nChild = 0;

    return t;
}

Node insert (int code, unsigned char k, int prune) {
    if (code < 0)
        return NULL;

    if (nCodes >= (1 << tSize)) {   // Resize Table
        if(tSize >= MAXBITS)        // Table @ Max Size
            return NULL;
        stringTable = realloc (stringTable, sizeof(Node)*(1 << ++tSize));
        memset (stringTable + (1 << (tSize-1)), 0, (1 << (tSize-1)));
    }
    // Create new Node
    Node prefix = stringTable[code];
    Node t = createNode (k, code, nCodes);
    
    // Insert Node
    stringTable[nCodes] = t;
    
    // Inserts Nodes into Prefix->Child using insertion sort
    if (prefix->nChild == 0) {                  // Initialize children array
        prefix->Child = malloc (sizeof(Node));
    }
    else {
        prefix->Child = realloc (prefix->Child, \
                sizeof(Node)*(prefix->nChild + 1));
    }
    prefix->Child[prefix->nChild++] = t;
    for (int j = 1; j < prefix->nChild; j++) {
        for (int i = j-1; i >= 0; i--) {
            Node temp, *R = &(prefix->Child[i+1]), *L = &(prefix->Child[i]);
            if ((*R)->K < (*L)->K) {          // Is Left child > Right Child? 
                temp = *L;                    // Swap L and R nodes
                *L = *R;
                *R = temp;
            }
            else break;     
        }
    }
    
    nCodes++;
    if (prune) {
        if (nCodes == (1 << MAXBITS) && PRUNE) {        // Full Table and Prune Flag
            pruneTable();
        }
    }

    return t;

}

int find (int code, unsigned char k) {
    Node prefix, temp = NULL;

    if (code >= (1 << tSize) || (prefix = stringTable[code]) == NULL)
        return -1;

    temp = binarySearch (k, prefix->Child, 0, prefix->nChild - 1); 

    if (temp == NULL)
        return -1;
    else {
        temp->Count++;
        return temp->Code;
    }
}

Node binarySearch (unsigned char key, Node *c, int l, int u) {
    if (l > u || c == NULL)
        return NULL;

    int m = (l + u) / 2;
    Node temp = c[m]; 
    if (temp->K == key)
        return temp;
    else if (temp->K > key)
        return binarySearch(key, c, l, m - 1);
    else
        return binarySearch(key, c, m + 1, u);
}

void pruneTable (void) {
    Node *table_copy = stringTable;
    Node oldT, newT;
    int n = nCodes, count, oldP, newP;

    /*printTable(stringTable);*/

    initializeTable(true);                          // Reset stringTable and stringTrie
    for (int i = nCodes; i < n; i++) {
        oldT = table_copy[i];
        count = oldT->Count / 2;                    // #appearance / 2 (int division)
        oldP = oldT->Prefix;
        if (count > 0 || (!ESCAPE && oldP == 0)) {
            newP = table_copy[oldP]->Code;
            newT = insert (newP,oldT->K, false);   // Check : what if you prune a table and you don't delete anything? Should you prune again? Probably not.
            newT->Count = count;
            freeNode (oldT);
            table_copy[i] = newT;
        }
        else 
            freeNode (oldT);
    }
    for (int i = 0; i < 3; i++) {               // Free SPECIAL codes of old stringTable
        freeNode (table_copy[i]);
    }
    free (table_copy);                          // Clean-up

    /*printTable(stringTable);*/

    return;
}

void freeNode (Node n) {
    if (n == NULL)
        return;
    free (n->Child);
    free (n);
}

void destroyTable () {
    for (int i = 0; i < nCodes; i++) {
        if (stringTable[i] != NULL)
            freeNode (stringTable[i]);
    }
    free (stringTable);
   
}

bool findCode (int code) {

    if (code == EMPTY || code == ESC || code == END || code < 0)
        exit (fprintf(stderr, "decode: invalid input\n"));

    if (ESCAPE && nCodes == 3 && code != ESC)
        exit (fprintf(stderr, "decode: invalid input\n"));

    if (code >= nCodes) {
        if (code > nCodes + 1)
            exit (fprintf(stderr, "decode: invalid input\n"));
        return false;
    }
    if (stringTable[code] == NULL)
        return false;
    return true;

}

unsigned char findK (int code, bool output) {
    if (code >= (1 << tSize))
        return code;

    Node temp = stringTable[code];
    if (temp == NULL)
        return code;

    if (output)
        temp->Count++;

    unsigned char finalK;
    if (temp->Prefix == EMPTY) {
        if (output)
            putchar (temp->K);
        return temp->K;
    }
    finalK = findK (temp->Prefix, output);
    if (output)
        putchar (temp->K);
    return finalK;
}

int codeToChar (int code, int look_ahead) {
   
    findK (code, true);
    if ((nCodes + look_ahead) >= (1 << MAXBITS) && PRUNE) {
        pruneTable();
        return EMPTY;
    }
    if (nCodes >= (1 << tSize)) {           // expands table after last code insertion
        if (tSize < MAXBITS) {
            stringTable = realloc (stringTable, sizeof(Node)*(1 << ++tSize));
            memset (stringTable + (1 << (tSize-1)), 0, (1 << (tSize-1))*sizeof(Node));
        }
    }
    return code;

}

void printTable (void) {
    //int i = 0;
    Node temp;
    printf("\ntSize: %d\nnCodes: %d\n",tSize,nCodes);
    printf("C     PREF     CHAR    COUNT\n");
    for (int i = 0; i < nCodes; i++) {
        temp = stringTable[i];
        printf("%5d %5d   %d (%c)    %5d\n", temp->Code, temp->Prefix,(int)temp->K, temp->K,temp->Count);
    }
}


int getcode (int nBits) {
    // Stage 3
    int temp;
    char k;
    nBits = (!nBits) ? tSize : nBits;
    switch (MODE) {
        case 3:
            return getBits (nBits);
            break;
        case 2:
    // Stage 2
            temp = 0;
            while (true) {
                if (isdigit((k = getchar()))){
                    temp = temp * 10;
                    temp = temp + k - '0';
                }
                else if (k == ':')
                    temp = 0;
                else
                    break;
            }
            if (k == EOF)
                return END;
            
            return temp;
            break;
        case 1:
    // Stage 1:
            temp = 0;
            while ( isdigit((k = getchar()))) {
                temp = temp * 10;
                temp = temp + k - '0';
            }
            if (k == EOF)
                return END;

            return temp;
            break;
    }
    return -1;
}

void output (int c, int nBits) {
    // Stage 3:
   switch (MODE) {
        case 3:
            nBits = (!nBits) ? tSize : nBits;
            putBits (nBits,c);
            break;
        case 2:
    // Stage 2:
            nBits = (!nBits) ? tSize : nBits;
            printf("%d:%d\n",nBits, c);
            break;
        case 1:
    // Stage 1:
            printf("%d\n", c);
            break;
   }
}

