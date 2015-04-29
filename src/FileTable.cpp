#include "FileTable.h"
#include "FileTree.h"
#include <string.h>
#include <io.h>
#include <stdio.h>
#include <Windows.h>
#include <sys/stat.h>
#include "tree.hh"

#define FHSIZE 32
#define NFS3_FHSIZE 64

static CFileTable g_FileTable;
static CFileTree g_FileTree;

CFileTable::CFileTable()
{
    m_pLastTable = m_pFirstTable = new FILE_TABLE;
    memset(m_pFirstTable, 0, sizeof(FILE_TABLE));
    m_nTableSize = 0;
    m_pCacheList = NULL;
}

CFileTable::~CFileTable()
{
    FILE_TABLE *pTable, *pTemp;
    unsigned int i;
    CACHE_LIST *pPrev;

    pTable = m_pFirstTable;

    while (pTable != NULL) { //free file table
        for (i = 0; i < TABLE_SIZE; i++) {
            delete[] pTable->pItems[i];
            delete[] pTable->pItems[i];
        }

        pTemp = pTable;
        pTable = pTable->pNext;
        delete pTemp;
    }

    while (m_pCacheList != NULL) { //free cache
        pPrev = m_pCacheList;
        m_pCacheList = m_pCacheList->pNext;
        delete pPrev;
    }
}

unsigned long CFileTable::GetIDByPath(char *path)
{
    unsigned char *handle;

    handle = GetHandleByPath(path);

    return *(unsigned long *)handle;
}

unsigned char *CFileTable::GetHandleByPath(char *path)
{
	tree_node_<FILE_ITEM> *node;

	node = g_FileTree.FindFileItemForPath(path);

	if (node == NULL) {
        AddItem(path);
		node = g_FileTree.FindFileItemForPath(path);
    }
	if (node == NULL) {
		return NULL;
	}

	return node->data.handle;
}

char *CFileTable::GetPathByHandle(unsigned char *handle)
{
	unsigned int id;
	tree_node_<FILE_ITEM>* node;

	id = *(unsigned int *)handle;
	node = GetItemByID(id);
	if (node != NULL) {
		return g_FileTree.GetNodeFullPath(node);
	}
	else {
		return NULL;
	}
    //return pItem == NULL ? NULL : pItem->path;
}

tree_node_<FILE_ITEM>* CFileTable::FindItemByPath(char *path)
{
	tree_node_<FILE_ITEM>* node= NULL;/*
    CACHE_LIST *pCurr;

    unsigned int i, j, nPathLen;
    FILE_TABLE *pTable;

    nPathLen = strlen(path);
    pItem = NULL;

    pCurr = m_pCacheList;

    while (pCurr != NULL) { //search in cache
        if (nPathLen == pCurr->pItem->nPathLen) { //comparing path length is faster than comparing path
            if (strcmp(path, pCurr->pItem->path) == 0) { //compare path
                pItem = pCurr->pItem;  //path matched
                break;
            }
        }

        pCurr = pCurr->pNext;
    }

    if (pItem == NULL) { //not found in cache
        pTable = m_pFirstTable;

        for (i = 0; i < m_nTableSize; i += TABLE_SIZE) { //search in file table
            for (j = 0; j < TABLE_SIZE; j++) {
                if (i + j >= m_nTableSize) { //all items in file table are compared
                    return NULL;
                }

                if (nPathLen == pTable->pItems[j].nPathLen) { //comparing path length is faster than comparing path
                    if (strcmp(path, pTable->pItems[j].path) == 0) { //compare path
                        pItem = pTable->pItems + j;  //path matched
                        break;
                    }
                }
            }

            if (pItem != NULL) {
                break;
            }

            pTable = pTable->pNext;
        }
    }

    if (pItem != NULL) {
        //TODO IMPLEMENTED CACHE RIGHT
        //PutItemInCache(pItem);  //put the found item in cache
    }
	*/
	return node;
}

tree_node_<FILE_ITEM>* CFileTable::AddItem(char *path)
{
    FILE_ITEM item;
	tree_node_<FILE_ITEM>* node;
    unsigned int nIndex;

	item.path = new char[strlen(path) + 1];
	strcpy_s(item.path, (strlen(path) + 1), path);  //path
	item.nPathLen = strlen(item.path);  //path length
	item.handle = new unsigned char[NFS3_FHSIZE];
	memset(item.handle, 0, NFS3_FHSIZE * sizeof(unsigned char));
	*(unsigned int *)item.handle = m_nTableSize;  //let its handle equal the index
	item.bCached = false;  //not in the cache

    if (m_nTableSize > 0 && (m_nTableSize & (TABLE_SIZE - 1)) == 0) {
        m_pLastTable->pNext = new FILE_TABLE;
        m_pLastTable = m_pLastTable->pNext;
        memset(m_pLastTable, 0, sizeof(FILE_TABLE));
    }

	g_FileTree.AddItem(path, item.handle);
	node = g_FileTree.FindFileItemForPath(path);

	m_pLastTable->pItems[nIndex = m_nTableSize & (TABLE_SIZE - 1)] = node;  //add the new item in the file table
    ++m_nTableSize;
	










	return node;  //return the pointer to the new item
}

tree_node_<FILE_ITEM>* CFileTable::GetItemByID(unsigned int nID)
{
	FILE_TABLE *pTable;
	unsigned int i;

	if (nID >= m_nTableSize) {
		return NULL;
	}

	pTable = m_pFirstTable;

	for (i = TABLE_SIZE; i <= nID; i += TABLE_SIZE) {
		pTable = pTable->pNext;
	}

	return pTable->pItems[nID + TABLE_SIZE - i];
}

void CFileTable::PutItemInCache(FILE_ITEM *pItem)
{
    CACHE_LIST *pPrev, *pCurr;
    int nCount;

    pPrev = NULL;
    pCurr = m_pCacheList;

    if (pItem->bCached) { //item is already in the cache
        while (pCurr != NULL) {
            if (pItem == pCurr->pItem) {
                if (pCurr == m_pCacheList) {  //at the first
                    return;
                }
                else {  //move to the first
                    pPrev->pNext = pCurr->pNext;
                    pCurr->pNext = m_pCacheList;
                    m_pCacheList = pCurr;
                    return;
                }
            }

            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }
    }
    else {
        pItem->bCached = true;

        for (nCount = 0; nCount < 9 && pCurr != NULL; nCount++) { //seek to the end of the cache
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }

        if (nCount == 9 && pCurr != NULL) { //there are 10 items in the cache
            pPrev->pNext = NULL;  //remove the last
            pCurr->pItem->bCached = false;
        }
        else {
            pCurr = new CACHE_LIST;
        }

        pCurr->pItem = pItem;
        pCurr->pNext = m_pCacheList;
        m_pCacheList = pCurr;  //insert to the first
    }
}

bool CFileTable::RemoveItem(char *path) {
   /* CACHE_LIST *pCurr;
    FILE_ITEM *pItem;
    unsigned int i, j, nPathLen;
    FILE_TABLE *pTable;
    int pItemIndex;

    nPathLen = strlen(path);
    pItem = NULL;

    bool foundDeletedItem = false;

    pCurr = m_pCacheList;

    while (pCurr != NULL) { //search in cache
        if (nPathLen == pCurr->pItem->nPathLen) { //comparing path length is faster than comparing path
            if (strcmp(path, pCurr->pItem->path) == 0) { //compare path
                pItem = pCurr->pItem;  //path matched
                break;
            }
        }

        pCurr = pCurr->pNext;
    }


    if (pItem != NULL) {
        //TODO IMPLEMENTED CACHE RIGHT
        //Remove item from cache
    }

    pTable = m_pFirstTable;

    for (i = 0; i < m_nTableSize; i += TABLE_SIZE) { //search in file table
        for (j = 0; j < TABLE_SIZE; j++) {
            if (i + j >= m_nTableSize) { //all items in file table are compared
                break;
            }

            if (!foundDeletedItem)
            {
                if (nPathLen == pTable->pItems[j].nPathLen) { //comparing path length is faster than comparing path
                    if (strcmp(path, pTable->pItems[j].path) == 0) { //compare path
                        foundDeletedItem = true;
                        memset(&(pTable->pItems[j]), 0, sizeof(FILE_ITEM));
                    }
                }
            }
        }

        pTable = pTable->pNext;
    }

    if (foundDeletedItem) {
		// we should not uncrement table size, because new file handle base on it
        //--m_nTableSize;
    }*/
	tree_node_<FILE_ITEM>* foundDeletedItem;
	foundDeletedItem = g_FileTree.FindFileItemForPath(path);
	g_FileTree.RemoveItem(path);
    return foundDeletedItem;
}

void CFileTable::RenameFile(char *pathFrom, char* pathTo)
{
	/*FILE_ITEM *pItem;

	pItem = g_FileTable.FindItemByPath(pathFrom);

	if (pItem == NULL) {
	} else {
		delete[] pItem->path;
		pItem->nPathLen = strlen(pathTo);
		pItem->path = new char[pItem->nPathLen + 1];
		strcpy_s(pItem->path, (pItem->nPathLen + 1), pathTo);  //replace the path by new one
	}
	*/
	g_FileTree.RenameItem(pathFrom, pathTo);
}

bool FileExists(char *path)
{
    int handle;
    struct _finddata_t fileinfo;

    handle = _findfirst(path, &fileinfo);
    _findclose(handle);

    return handle == -1 ? false : strcmp(fileinfo.name, strrchr(path, '\\') + 1) == 0;  //filename must match case
}

unsigned long GetFileID(char *path)
{
    return g_FileTable.GetIDByPath(path);
}

unsigned char *GetFileHandle(char *path)
{
    return g_FileTable.GetHandleByPath(path);
}

char *GetFilePath(unsigned char *handle)
{
    return g_FileTable.GetPathByHandle(handle);
}

int RenameFile(char *pathFrom, char *pathTo)
{
	tree_node_<FILE_ITEM>* node;
    FILE_ITEM *pItem;

	node = g_FileTable.FindItemByPath(pathFrom);
    pItem = &(node->data);

    if (pItem == NULL) {
        return false;
    }

    errno_t errorNumber = rename(pathFrom, pathTo);

    if (errorNumber == 0) { //success
		g_FileTable.RenameFile(pathFrom, pathTo);
        return errorNumber;
    }
    else {
        return errorNumber;
    }
}

int RenameDirectory(char *pathFrom, char *pathTo)
{
	errno_t errorNumber = RenameFile(pathFrom, pathTo);

	char* dotFile = "\\.";
	char* backFile = "\\..";

	char* dotDirectoryPathFrom;
	char* dotDirectoryPathTo;
	char* backDirectoryPathFrom;
	char* backDirectoryPathTo;
	dotDirectoryPathFrom = (char *)malloc(strlen(pathFrom) + 1 + 3);
	strcpy_s(dotDirectoryPathFrom, (strlen(pathFrom) + 1), pathFrom);
	strcat_s(dotDirectoryPathFrom, (strlen(pathFrom) + 5), dotFile);

	dotDirectoryPathTo = (char *)malloc(strlen(pathTo) + 1 + 3);
	strcpy_s(dotDirectoryPathTo, (strlen(pathTo) + 1), pathTo);
	strcat_s(dotDirectoryPathTo, (strlen(pathTo) + 5), dotFile);

	backDirectoryPathFrom = (char *)malloc(strlen(pathFrom) + 1 + 4);
	strcpy_s(backDirectoryPathFrom, (strlen(pathFrom) + 1), pathFrom);
	strcat_s(backDirectoryPathFrom, (strlen(pathFrom) + 6), backFile);

	backDirectoryPathTo = (char *)malloc(strlen(pathTo) + 1 + 4);
	strcpy_s(backDirectoryPathTo, (strlen(pathTo) + 1), pathTo);
	strcat_s(backDirectoryPathTo, (strlen(pathTo) + 6), backFile);

	g_FileTable.RenameFile(dotDirectoryPathFrom, dotDirectoryPathTo);
	g_FileTable.RenameFile(backDirectoryPathFrom, backDirectoryPathTo);
	return errorNumber;


}

bool RemoveFile(char *path)
{
	int nMode = 0;
	nMode |= S_IREAD;
	nMode |= S_IWRITE;
	_chmod(path, nMode);

    if (remove(path) == 0){
        g_FileTable.RemoveItem(path);
        return true;
    }
    return false;
}

bool RemoveFolder(char *path)
{
	int nMode = 0;
	nMode |= S_IREAD;
	nMode |= S_IWRITE;
	_chmod(path, nMode);

    if (RemoveDirectory(path) != 0) {
        g_FileTable.RemoveItem(path);
        char* dotFile = "\\.";
        char* backFile = "\\..";

        char* dotDirectoryPath;
        char* backDirectoryPath;
        dotDirectoryPath = (char *)malloc(strlen(path) + 1 + 3);
        strcpy_s(dotDirectoryPath, (strlen(path) + 1), path);
        strcat_s(dotDirectoryPath, (strlen(path) + 5), dotFile);
        backDirectoryPath = (char *)malloc(strlen(path) + 1 + 4);
        strcpy_s(backDirectoryPath, (strlen(path) + 1), path);
        strcat_s(backDirectoryPath, (strlen(path) + 6), backFile);

        g_FileTable.RemoveItem(dotDirectoryPath);
        g_FileTable.RemoveItem(backDirectoryPath);
        return true;
    }
    return false;
}

void RemovePathFromFileTable(char *path)
{
    g_FileTable.RemoveItem(path);
}
