/******************************************************************************
 *
 * Routine to open a new file.  
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to 
 *     by XIAHOME.
 * Finally try to open the file as an environment variable.
 *
 ******************************************************************************/
static FILE* dxp_find_file(const char* filename, const char* mode, char newFile[MAXFILENAME_LEN])
/* const char *filename;			Input: filename to open			*/
/* const char *mode;				Input: Mode to use when opening	*/
/* char *newFile;					Output: Full filename of file (translated env vars)	*/
{
	FILE *fp=NULL;
	char *name=NULL, *name2=NULL;
	char *home=NULL;
	
	unsigned int len = 0;

	

/* Try to open file directly */
	if((fp=fopen(filename,mode))!=NULL){
		len = MAXFILENAME_LEN>(strlen(filename)+1) ? strlen(filename) : MAXFILENAME_LEN;
		strncpy(newFile, filename, len);
		newFile[len] = '\0';
		return fp;
	}
/* Try to open the file with the path XIAHOME */
	if ((home=getenv("XIAHOME"))!=NULL) {
		name = (char *) dxp_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=fopen(name,mode))!=NULL){
			len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
			strncpy(newFile, name, len);
			newFile[len] = '\0';
			dxp_md_free(name);
			return fp;
		}
		dxp_md_free(name);
		name = NULL;
	}
/* Try to open the file with the path DXPHOME */
	if ((home=getenv("DXPHOME"))!=NULL) {
		name = (char *) dxp_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=fopen(name,mode))!=NULL){
			len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
			strncpy(newFile, name, len);
			newFile[len] = '\0';
			dxp_md_free(name);
			return fp;
		}
		dxp_md_free(name);
		name = NULL;
	}
/* Try to open the file as an environment variable */
	if ((name=getenv(filename))!=NULL) {
		if((fp=fopen(name,mode))!=NULL){
			len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
			strncpy(newFile, name, len);
			newFile[len] = '\0';
			return fp;
		}
		name = NULL;
	}
/* Try to open the file with the path XIAHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("XIAHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) dxp_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=fopen(name,mode))!=NULL) {
				len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
				strncpy(newFile, name, len);
				newFile[len] = '\0';
				dxp_md_free(name);
				return fp;
			}
			dxp_md_free(name);
			name = NULL;
		}
	}
/* Try to open the file with the path DXPHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("DXPHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) dxp_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=fopen(name,mode))!=NULL) {
				len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
				strncpy(newFile, name, len);
				newFile[len] = '\0';
				dxp_md_free(name);
				return fp;
			}
			dxp_md_free(name);
			name = NULL;
		}
	}

	return NULL;
}

