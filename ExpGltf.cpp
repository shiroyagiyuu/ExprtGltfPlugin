#include "stdafx.h"
#include "MQBasePlugin.h"
#include "MQWidget.h"
#include "MQSetting.h"
#include "MQ3DLib.h"
#include "JSonWriter.h"
#include <string>
#include <vector>
#include <map>
#include <string.h>
#include <wchar.h>
#if __APPLE__
#define MAX_PATH PATH_MAX
#define sprintf_s sprintf
#endif

#ifdef WIN32
extern HINSTANCE s_hInstance;
#endif

#define DEBUG 0
#if DEBUG
FILE* dbgf;
#define dbg_pr(fmt,...)	{ fprintf(dbgf, fmt, __VA_ARGS__); fflush(dbgf); }
#else
#define dbg_pr(fmt,...)	;
#endif

#define MULTI_BUFFER 0
#define INDC_INT 1
#if INDC_INT
typedef unsigned int INDICES;
#else
typedef unsigned short INDICIES;
#endif

const int base64_header_len = 38;
const char base64_header_cont[] = "data:application/octet-stream;base64,";

const char unattached_material_name[] = "_Unattached";

static void base64Encode(const char* src, char* dtc, int len, int* dtc_len) {
	//	6bitからbase64の文字へ変換するテーブル
	//                              1         2         3         4         5         6
	//                    0123456789012345678901234567890123456789012345678901234567890123456789
	//                                    1               2               3
	//                    0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF
	const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int n;
	int mod = len % 3;

	int adj_len = len - mod;	//	3の倍数に修正
	char* p = dtc;
	int o0, o1, o2, o3;

	for (n = 0; n < adj_len;) {
		unsigned int x = (((unsigned int)src[n] << 16) & 0xFF0000) +
						(((unsigned int)src[n + 1] << 8) & 0x00FF00) +
						(((unsigned int)src[n + 2]) & 0x0000FF);
		o0 = (x >> 18) & 0x3f;
		o1 = (x >> 12) & 0x3f;
		o2 = (x >> 6) & 0x3f;
		o3 = x & 0x3f;
		*p++ = table[o0];
		*p++ = table[o1];
		*p++ = table[o2];
		*p++ = table[o3];
		n += 3;
	}
	if (mod) {
		if (mod == 1) {
			unsigned int x = (unsigned int)src[n] << 16;
			o0 = (x >> 18) & 0x3f;
			o1 = (x >> 12) & 0x3f;
			*p++ = table[o0];
			*p++ = table[o1];
			*p++ = '=';
			*p++ = '=';
		}
		else if (mod == 2) {
			unsigned int x = ((unsigned int)src[n] << 16) + ((unsigned int)src[n + 1] << 8);
			o0 = (x >> 18) & 0x3f;
			o1 = (x >> 12) & 0x3f;
			o2 = (x >> 6) & 0x3f;
			*p++ = table[o0];
			*p++ = table[o1];
			*p++ = table[o2];
			*p++ = '=';
		}
	}
	*p = 0;
	*dtc_len = (int)(p - dtc);
}

static std::string GetFileName(const wchar_t* fname)
{
	std::vector<wchar_t>  tmp = std::vector<wchar_t>(256);
	const wchar_t* en = wcsrchr(fname, L'.');
	const wchar_t* st = wcsrchr(fname, L'\\');
	int  i;

	if (st == NULL) {
		st = fname;
	} else {
		st++;
	}

	for (i = 0; (i < 255) && (st != en); i++) {
		tmp[i] = *st;
		st++;
	}
	tmp[i] = L'\0';
	std::string  result = MQEncoding::WideToUtf8(tmp.data());

	return result;
}

class Primitive
{
public:
	Primitive(int mat_idx);

	void AddFace(int pt1, int pt2, int pt3);
	int GetMaterialIndex();
	INDICES* GetFaceBuffer();
	INDICES GetPoint(int idx);
	int GetPointCount();

private:
	int  mat_idx;
	std::vector<INDICES>  indices;
};

Primitive::Primitive(int idx)
{
	this->mat_idx = idx;
}

int Primitive::GetMaterialIndex()
{
	return this->mat_idx;
}

void Primitive::AddFace(int pt1, int pt2, int pt3)
{
	indices.push_back(pt1);
	indices.push_back(pt2);
	indices.push_back(pt3);
}

INDICES* Primitive::GetFaceBuffer()
{
	return &indices[0];
}

int Primitive::GetPointCount()
{
	return this->indices.size();
}

INDICES Primitive::GetPoint(int idx)
{
	return this->indices[idx];
}

class Texture
{
public:
	Texture(int _mat_idx, int _img_idx, int _smp_idx);

	int GetMaterial();
	int GetImage();
	int GetSampler();

private:
	int  mat_idx;
	int  img_idx;
	int  smp_idx;
};

Texture::Texture(int _mat_idx, int _img_idx, int _smp_idx) {
	this->mat_idx = _mat_idx;
	this->img_idx = _img_idx;
	this->smp_idx = _smp_idx;
}

int Texture::GetMaterial() {
	return this->mat_idx;
}

int Texture::GetImage() {
	return this->img_idx;
}

int Texture::GetSampler() {
	return this->smp_idx;
}

class Sampler
{
public:
	Sampler();
	Sampler(int _filter, int _wrapS, int _wrapT);

	int  filter;
	int  wrapS, wrapT;
};

Sampler::Sampler()
{
	this->filter = 0;
	this->wrapS = this->wrapT = 0;
}

Sampler::Sampler(int _filter, int _wrapS, int _wrapT)
{
	this->filter = _filter;
	this->wrapS = _wrapS;
	this->wrapT = 0;
}

enum buftype {
	BUFTYPE_VECTOR,
	BUFTYPE_SCALAR,
	BUFTYPE_ALL
};

class VertexStore
{
public:
	VertexStore() {};
	~VertexStore() {};

	int AddPoint(MQPoint& position, MQPoint& normal, MQCoordinate& tex_coord);
	int GetNextPointIndices();

	int GetPositionSize();
	int GetNormalSize();
	int GetTexCoordSize();

	unsigned int AddFace(unsigned int mat_index, int pt1, int pt2, int pt3);
	int GetMeshOffset(int num);
	int GetMeshLength(int num);
	int GetMaterialIndex(int num);
	int GetMeshNum();

	bool IsUshortIndices();
	int GetIndicesSize();
	int GetBufferLength();
	int GetBufferContent(std::vector<char>& buffer);
#if MULTI_BUFFER
	int GetBufferContent(std::vector<char>& buffer, enum buftype buf_type);
#endif

	int GetVectorDataSize();
	int GetScalarDataSize();

	int AddTexture(int mat_idx, char *tex_file, int filter, int wrapS, int wrapT);
	Texture *GetTexture(int idx);
	int GetTextureNum();
	Texture *GetTextureFromMat(int mat_idx);

	int GetImageNum();
	std::string GetImageUri(int idx);
	int GetSamplerNum();
	Sampler* GetSampler(int idx);

private:
	int AddImage(std::string& _img);
	int AddSampler(int _filter, int _wrapS, int _wrapT);

	std::vector<MQPoint>		positions;
	std::vector<MQPoint>		normals;
	std::vector<MQCoordinate>	tex_coords;

	std::vector<Primitive>	 primitives;
	std::vector<Texture>     textures;
	std::vector<std::string> img_uris;
	std::vector<Sampler>     samplers;
};

int VertexStore::AddPoint(MQPoint& position, MQPoint& normal, MQCoordinate& tex_coord)
{
	int  idx;

	for (idx = 0; idx < positions.size(); idx++) {
		if ((positions[idx] == position) &&
			(normals[idx] == normal) &&
			(tex_coords[idx] == tex_coord)) {
			return idx;
		}
	}

	positions.push_back(position);
	normals.push_back(normal);
	tex_coords.push_back(tex_coord);

	return idx;
}

int VertexStore::GetNextPointIndices()
{
	return positions.size();
}

int VertexStore::GetPositionSize()
{
	return positions.size();
}

int VertexStore::GetNormalSize()
{
	return normals.size();
}

int VertexStore::GetTexCoordSize()
{
	return tex_coords.size();
}

unsigned int VertexStore::AddFace(unsigned int mat_index, int pt1, int pt2, int pt3)
{
	unsigned int  m;
	for (m = 0; m < primitives.size(); m++) {
		if (mat_index == primitives[m].GetMaterialIndex()) {
			primitives[m].AddFace(pt1, pt2, pt3);
			return m;
		}
	}

	Primitive  new_prim(mat_index);
	new_prim.AddFace(pt1, pt2, pt3);
	primitives.push_back(new_prim);

	return m;
}

int VertexStore::GetMeshOffset(int num)
{
	int  offset = 0;
	for (int i = 0; i < num; i++) {
		offset += primitives[i].GetPointCount();
	}
	return offset;
}

int VertexStore::GetMeshLength(int num)
{
	return primitives[num].GetPointCount();
}

int VertexStore::GetMaterialIndex (int num)
{
	return this->primitives[num].GetMaterialIndex();
}

int VertexStore::GetMeshNum()
{
	return this->primitives.size();
}

int VertexStore::GetBufferLength()
{
	return this->GetVectorDataSize() + this->GetScalarDataSize();
}

bool VertexStore::IsUshortIndices()
{
	return (positions.size() < 0xFFFF);
}

int VertexStore::GetIndicesSize()
{
	return (IsUshortIndices() ? sizeof(unsigned short) : sizeof(unsigned int));
}

int VertexStore::GetBufferContent(std::vector<char>& buffer_vec)
{
	char* buffer = &buffer_vec[0];
	int  pos_len = positions.size() * 3 * sizeof(float);
	int  nrm_len = normals.size() * 3 * sizeof(float);
	int  tex_len = tex_coords.size() * 2 * sizeof(float);
	int  ind_len = 0;
	
	for (int i = 0; i < primitives.size(); i++) {
		ind_len += primitives[i].GetPointCount();
	}
	ind_len *= GetIndicesSize();

	memcpy(buffer, &positions[0], pos_len);
	buffer += pos_len;
	memcpy(buffer, &normals[0], nrm_len);
	buffer += nrm_len;
	memcpy(buffer, &tex_coords[0], tex_len);
	buffer += tex_len;

	if (IsUshortIndices()) {
		unsigned short* s = (unsigned short*)buffer;
		for (int i = 0; i < primitives.size(); i++) {
			for (int j = 0; j < primitives[i].GetPointCount(); j++) {
				*s = (unsigned short)(primitives[i].GetPoint(j) & 0xFFFF);
				s++;
			}
		}
	}
	else {
		for (int i = 0; i < primitives.size(); i++) {
			int  msz = primitives[i].GetPointCount() * sizeof(INDICES);
			memcpy(buffer, primitives[i].GetFaceBuffer(), msz);
			buffer += msz;
		}
	}

	dbg_pr("buftype(ALL) size=%d\n", pos_len + nrm_len + tex_len + ind_len);

	return pos_len + nrm_len + tex_len + ind_len;
}

#if MULTI_BUFFER
int VertexStore::GetBufferContent(char* buffer, enum buftype buf_type)
{
	int total_len = 0;

	if (BUFTYPE_VECTOR == buf_type || BUFTYPE_ALL == buf_type) {
		int  pos_len = positions.size() * 3 * sizeof(float);
		int  nrm_len = normals.size() * 3 * sizeof(float);
		int  tex_len = tex_coords.size() * 2 * sizeof(float);

		memcpy(buffer, &positions[0], pos_len);
		buffer += pos_len;
		memcpy(buffer, &normals[0], nrm_len);
		buffer += nrm_len;
		memcpy(buffer, &tex_coords[0], tex_len);
		buffer += tex_len;

		total_len += pos_len + nrm_len + tex_len;
	}

	if (BUFTYPE_SCALAR==buf_type || BUFTYPE_ALL ==buf_type) {
		int  ind_len = 0;
		int  i;
		
		for (i = 0; i < primitives.size(); i++) {
			int  msz = primitives[i].GetPointCount() * sizeof(INDICIES);
			memcpy(buffer, primitives[i].GetFaceBuffer(), msz);
			buffer += msz;

			total_len += msz;
		}
	}

	dbg_pr("buftype(%d) size=%d\n", buf_type, total_len);

	return  total_len;
}
#endif

int VertexStore::GetVectorDataSize()
{
	return (this->positions.size() * 3 * 4)
		+ (this->normals.size() * 3 * 4)
		+ (this->tex_coords.size() * 2 * 4);
}

int VertexStore::GetScalarDataSize()
{
	int  total = 0;

	for (int i = 0; i < primitives.size(); i++) {
		total += this->primitives[i].GetPointCount();
	}
	return  total * GetIndicesSize();
}

int VertexStore::AddImage(std::string& _uri)
{
	int  i;

	for (i = 0; i < img_uris.size(); i++) {
		if (img_uris[i] == _uri) {
			return i;
		}
	}

	img_uris.push_back(_uri);	//new img
	return i;
}

int VertexStore::GetImageNum()
{
	return this->img_uris.size();
}

std::string VertexStore::GetImageUri(int idx)
{
	return this->img_uris[idx];
}

int VertexStore::AddSampler(int _filter, int _wrapS, int _wrapT)
{
	int  i;

	for (i = 0; i < samplers.size(); i++) {
		if ((samplers[i].filter == _filter) &&
			(samplers[i].wrapS == _wrapS) &&
			(samplers[i].wrapT == _wrapT)) {
			return i;
		}
	}

	Sampler  nsamp(_filter, _wrapS, _wrapT);
	samplers.push_back(nsamp);	//new sampler
	return i;
}

int VertexStore::GetSamplerNum()
{
	return this->samplers.size();
}

Sampler* VertexStore::GetSampler(int idx)
{
	return &(this->samplers[idx]);
}

int VertexStore::AddTexture(int mat_idx, char *tex_file_ascii, int filter, int wrapS, int wrapT)
{
	std::string texfile_utf8 = MQEncoding::AnsiToUtf8(tex_file_ascii);

	int  img_idx = AddImage(texfile_utf8);
	int  smp_idx = AddSampler(filter, wrapS, wrapT);
	int  num;

	for (num = 0; num < textures.size(); num++) {
		if ((textures[num].GetImage() == img_idx) &&
			(textures[num].GetSampler() == smp_idx)) {
			return num;
		}
	}

	Texture tex(mat_idx, img_idx, smp_idx);
	textures.push_back(tex);

	return num;
}

Texture* VertexStore::GetTexture(int idx)
{
	return &textures[idx];
}

int VertexStore::GetTextureNum()
{
	return textures.size();
}

Texture* VertexStore::GetTextureFromMat(int mat_idx)
{
	for (int i = 0; i < this->textures.size(); i++) {
		if (textures[i].GetMaterial() == mat_idx) {
			return &textures[i];
		}
	}
	dbg_pr("Texture Not Found! %d\n", mat_idx);
	return NULL;
}

MQPoint GetMinPoint(MQPoint pt1, MQPoint pt2)
{
	MQPoint  ret;
	ret.x = min(pt1.x, pt2.x);
	ret.y = min(pt1.y, pt2.y);
	ret.z = min(pt1.z, pt2.z);

	return ret;
}

MQPoint GetMaxPoint(MQPoint pt1, MQPoint pt2)
{
	MQPoint  ret;
	ret.x = max(pt1.x, pt2.x);
	ret.y = max(pt1.y, pt2.y);
	ret.z = max(pt1.z, pt2.z);

	return ret;
}

class GltfExporter : public MQExportPlugin
{
public:
	GltfExporter();
	~GltfExporter();

	// プラグインIDを返す。
	virtual void GetPlugInID(DWORD *Product, DWORD *ID) override;

	// プラグイン名を返す。
	virtual const char *GetPlugInName(void) override;

	// 入力または出力可能なファイルタイプを返す。
	virtual const char *EnumFileType(int index) override;

	// 入力または出力可能な拡張子を返す。
	virtual const char *EnumFileExt(int index) override;

	// ファイルの出力
	virtual BOOL ExportFile(int index, const wchar_t *filename, MQDocument doc) override;

private:
	BOOL ExportGltfText(const wchar_t *filename, MQDocument doc);
	void ExportTexture(JSonWriter *wtr, VertexStore *store, int mat_idx, MQMaterial mat, MQDocument doc);

	bool m_bLocal;
};


GltfExporter::GltfExporter()
{
	m_bLocal = true;
}

GltfExporter::~GltfExporter()
{
}

// プラグインIDを返す。
void GltfExporter::GetPlugInID(DWORD *Product, DWORD *ID)
{
	*Product = 0x56120000;
	*ID = 0x5121F4A1;
}

// プラグイン名を返す。
const char *GltfExporter::GetPlugInName(void)
{
	return "Gltf Exporter   Copyright(C) 2019, PurePlus.";
}


// 入力または出力可能なファイルタイプを返す。
const char *GltfExporter::EnumFileType(int index)
{
	switch(index){
	case 0: return "glTF 2.0 Text (*.gltf)";
	}
	return NULL;
}

// 入力または出力可能な拡張子を返す。
const char *GltfExporter::EnumFileExt(int index)
{
	switch(index){
	case 0: return "gltf";
	}
	return NULL;
}

// ファイルの出力
BOOL GltfExporter::ExportFile(int index, const wchar_t *filename, MQDocument doc)
{
	if(index == 0){
		return ExportGltfText(filename, doc);
	}
	return FALSE;
}


struct CreateDialogOptionParam
{
	GltfExporter *plugin;
	MQDialog *dialog;
	MQCheckBox *check_binary;
	
	bool exp_binary;
};

static void CreateDialogOption(bool init, MQFileDialogCallbackParam *param, void *ptr)
{
	CreateDialogOptionParam *option = (CreateDialogOptionParam*)ptr;
	
	if(init)
	{
		option->dialog = new MQDialog(param->dialog_id);
		MQFrame parent(param->parent_frame_id);
	
		MQGroupBox *group = option->dialog->CreateGroupBox(&parent, L"Option");

		option->check_binary = option->dialog->CreateCheckBox(group);
		option->check_binary->SetText(L"Export Buffer as Binary");
		option->check_binary->SetChecked(option->exp_binary);
	}
	else
	{
		option->exp_binary = option->check_binary->GetChecked();
		delete option->dialog;
	}
}

static int path2uri(char* orig_dst, const char* src, int len)
{
	char *dst = orig_dst;

	if (src[0] == '\0') {
		*dst = *src;
		return 0;
	}

	if (src[1] == ':' && src[2] == '\\') {	//fullpath is ng?
		strncpy(dst, "file:///", 8);
		dst += 8;
		len -= 8;
	}

	for (int i = 0; i < len; i++) {
		if (*src == '\\') {
			*dst = '/';
		}
		else {
			*dst = *src;
		}
		src++;
		dst++;
	}

	return dst - orig_dst;
}

void GltfExporter::ExportTexture(JSonWriter *wtr, VertexStore *store, int mat_idx, MQMaterial mat, MQDocument doc)
{
	dbg_pr("texture:");
	dbg_pr("\tMAP(%d)=%d\n", mat_idx, mat->GetMappingType());
	//if (mat->GetMappingType() == MQMATERIAL_PROJECTION_UV) {
	std::vector<char> texname = std::vector<char>(512);
	std::vector<char> uri = std::vector<char>(512);

	mat->GetTextureName(&texname[0], 512);
	path2uri(&uri[0], &texname[0], 512);
	dbg_pr("\tfname:[%s]\n", texname);
	dbg_pr("\turi  :[%s]\n", uri);

	if (texname[0] != '\0') {
		int  filter = mat->GetMappingFilter();
		int  wraps = mat->GetWrapModeU();
		int  wrapt = mat->GetWrapModeV();

		int tex_num = store->AddTexture(mat_idx, uri.data(), filter, wraps, wrapt);
		wtr->beginObject("baseColorTexture");
		wtr->node("index", tex_num);
		wtr->node("texCoord", 0);
		wtr->endObject();
	}
	//}
}

static void ExportBuffer(JSonWriter& wtr, std::string& name, std::vector<char>& buffer, int len, bool text_mode)
{
	wtr.beginObject();
	wtr.node("name", name.c_str());
	wtr.node("byteLength", len);

	if (text_mode) {
		int  str_len = (len * 3 / 2) + base64_header_len + 1;
		std::vector<char> str_data = std::vector<char>(str_len); //stringにしたい
		snprintf(&str_data[0], str_len, base64_header_cont);
		base64Encode(buffer.data(), &str_data[base64_header_len - 1], len, &str_len);
		wtr.node("uri", str_data.data());
	}
	else {
		std::string binpath = std::string(name);
		binpath += ".bin";
		FILE* binf = fopen(binpath.c_str(), "wb");
		fwrite(buffer.data(), len, 1, binf);
		fclose(binf);
		wtr.node("uri", binpath.c_str());
	}

	wtr.endObject();
}

BOOL GltfExporter::ExportGltfText(const wchar_t *filename, MQDocument doc)
{
	float scaling = 1e-3f;
	bool  buftype_bin = false;

	CreateDialogOptionParam option;
	option.plugin = this;
	option.exp_binary = true;

	// Load a setting.
	MQSetting *setting = OpenSetting();
	if(setting != NULL){
		setting->Load("Scaling", scaling, scaling);
		setting->Load("ExpBin", option.exp_binary, option.exp_binary);
	}

	// Show a dialog for converting axes
	// 座標軸変換用ダイアログの表示
	MQFileDialogInfo dlginfo;
	memset(&dlginfo, 0, sizeof(dlginfo));
	dlginfo.dwSize = sizeof(dlginfo);
	dlginfo.hidden_flag = MQFileDialogInfo::HIDDEN_AXIS | MQFileDialogInfo::HIDDEN_INVERT_FACE;
	dlginfo.scale = scaling;
	dlginfo.axis_x = MQFILE_TYPE_RIGHT;
	dlginfo.axis_y = MQFILE_TYPE_UP;
	dlginfo.axis_z = MQFILE_TYPE_FRONT;
	dlginfo.softname = "";
	dlginfo.dialog_callback = CreateDialogOption;
	dlginfo.dialog_callback_ptr = &option;
	MQ_ShowFileDialog("GLTF Export", &dlginfo);

	scaling = dlginfo.scale;
	buftype_bin = option.exp_binary;

	// Save a setting.
	if(setting != NULL){
		setting->Save("Scaling", scaling);
		setting->Save("ExpBin", option.exp_binary);
		CloseSetting(setting);
	}

	MQWaitCursorChanger wait_cursor(this);

	// Create a file.
	JSonWriter  wtr;
	wtr.open(filename);

	std::string  fname_top = GetFileName(filename);

#if DEBUG
	dbgf = fopen("debug.txt", "w");
#endif

#ifdef WIN32
	SYSTEMTIME tm;
	GetSystemTime(&tm);
#else
	time_t timer = time(NULL);
	struct tm *tm;
	tm = localtime(&timer);
#endif

	VertexStore  store;

	wtr.beginObject();

	int numObj = doc->GetObjectCount();
	int numMat = doc->GetMaterialCount();

	int position_idx=0;
	int valid_mesh_num=0;

#if 0
	// Check if the material is used
	std::vector<bool> material_used((long long)numMat+1, false);
	for(int obj_idx = 0; obj_idx < numObj; obj_idx++)
	{
		MQObject obj = doc->GetObject(obj_idx);
		if(obj == NULL){
			continue;
		}

		int numFace = obj->GetFaceCount();
		for(int face_idx = 0; face_idx < numFace; face_idx++)
		{
			int pt_num = obj->GetFacePointCount(face_idx);
			if(pt_num > 0){
				int mat_idx = obj->GetFaceMaterial(face_idx);
				if(mat_idx >= 0 && mat_idx < numMat){
					material_used[mat_idx] = true;
				}else{
					material_used[numMat] = true;
				}
			}
		}
	}
#endif

	wtr.beginArray("accessors");
	// Geometries
	// Positions
	{
		MQPoint  pos_min, pos_max;
		MQPoint  nrm_min, nrm_max;
		pos_min.x = pos_min.y = pos_min.z = pos_max.x = pos_max.y = pos_max.z = 0.0f;

		for (int obj_idx = 0; obj_idx < numObj; obj_idx++)
		{
			MQObject orig_obj = doc->GetObject(obj_idx);
			if (orig_obj == NULL) {
				continue;
			}

			int  face_offset = 0;

			if (orig_obj->GetVisible() == 0) continue;
			if (orig_obj->GetFaceCount() == 0) continue;
			
			MQObject  obj = orig_obj->Clone();
			obj->SetLocking(FALSE);
			bool frz = obj->Freeze(MQOBJECT_FREEZE_PATCH | MQOBJECT_FREEZE_MIRROR | MQOBJECT_FREEZE_LATHE);
			dbg_pr("freeze:%d\n", frz);
			int numFace = obj->GetFaceCount();

			char obj_name[64];
			obj->GetName(obj_name, _countof(obj_name));
			std::string obj_name_utf8 = MQEncoding::AnsiToUtf8(obj_name);

			MQMatrix invmat;
			doc->GetGlobalInverseMatrix(obj, invmat);

			obj->UpdateNormal();
			
			for (int face_idx = 0; face_idx < numFace; face_idx++)
			{
				int pt_num = obj->GetFacePointCount(face_idx);
				if (pt_num < 3) {
					dbg_pr("skip face(%d)\n", pt_num); continue;
				}

				std::vector<int> points = std::vector<int>(pt_num);
				std::vector<MQPoint> pos_ary = std::vector<MQPoint>(pt_num);
				std::vector<int> ind_ary = std::vector<int>(pt_num);

				obj->GetFacePointArray(face_idx, &points[0]);

				face_offset = store.GetNextPointIndices();

				for (int pt = 0; pt < pt_num; pt++) {
					MQPoint pos = obj->GetVertex(points[pt]) * scaling;
					MQPoint nrm;
					BYTE    flag;
					obj->GetFaceVertexNormal(face_idx, pt, flag, nrm);
					MQCoordinate  tex_cood;
					obj->GetFaceCoordinate(face_idx, pt, tex_cood);

#if 0
					if (m_bLocal) {
						pos = pos * invmat;
						nrm = invmat.Mult3(nrm);
						nrm.normalize();
					}
#endif

					pos_min = GetMinPoint(pos_min, pos);
					pos_max = GetMaxPoint(pos_max, pos);
					nrm_min = GetMinPoint(nrm_min, nrm);
					nrm_max = GetMaxPoint(nrm_max, nrm);

					ind_ary[pt] = store.AddPoint(pos, nrm, tex_cood);
					pos_ary[pt] = pos;
				}

#if 1
				{
					int  tri_len = (pt_num - 2) * 3;
					std::vector<int> tri_idxs = std::vector<int>(tri_len);

					doc->Triangulate(&pos_ary[0], pt_num, &tri_idxs[0], tri_len);
					int  mat_idx = obj->GetFaceMaterial(face_idx);
					for (int i = 0; i < tri_len; i += 3) {
						store.AddFace(mat_idx, ind_ary[tri_idxs[i]],
							ind_ary[tri_idxs[i + 2]],
							ind_ary[tri_idxs[i + 1]]);
					}
				}
#else

				store.AddFace(mat_idx, face_offset,
									face_offset+2,
									face_offset+1);

				for (int pt = 3; pt < pt_num; pt++) {
					store.AddFace(mat_idx, face_offset + pt - 1,
										face_offset,
										face_offset + pt);
				}
#endif
			}

			obj->DeleteThis();
			valid_mesh_num++;
		}

#define GL_FLOAT                          0x1406
		wtr.beginObject();
		wtr.node("name", "Node-Mesh_positions");
		wtr.node("componentType", GL_FLOAT);
		wtr.node("count", store.GetPositionSize());
		wtr.writeArray("min", pos_min.x, pos_min.y, pos_min.z);
		wtr.writeArray("max", pos_max.x, pos_max.y, pos_max.z);
		wtr.node("type", "VEC3");
		wtr.node("bufferView", 0);
		wtr.node("byteOffset", 0);
		wtr.endObject();

		wtr.beginObject();
		wtr.node("name", "Node-Mesh_normals");
		wtr.node("componentType", GL_FLOAT);
		wtr.node("count", store.GetNormalSize());
		wtr.writeArray("min", nrm_min.x, nrm_min.y, nrm_min.z);
		wtr.writeArray("max", nrm_max.x, nrm_max.y, nrm_max.z);
		wtr.node("type", "VEC3");
		wtr.node("bufferView", 1);
		wtr.node("byteOffset", 0);
		wtr.endObject();

		//TexCoords		
		wtr.beginObject();
		wtr.node("name", "Node-Mesh_texcoords");
		wtr.node("componentType", GL_FLOAT);
		wtr.node("count", store.GetTexCoordSize());
		wtr.writeArray("min", 0, 0);
		wtr.writeArray("max", 1, 1);	//TODO: really?
		wtr.node("type", "VEC2");
		wtr.node("bufferView", 2);
		wtr.node("byteOffset", 0);
		wtr.endObject();

		for (int i=0; i < store.GetMeshNum(); i++) {
			wtr.beginObject();
			char  tmp[32];
			snprintf(tmp, 32, "Node-Mesh_%d_indices", i);
			wtr.node("name", tmp);
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_UNSIGNED_INT                   0x1405
			if (store.IsUshortIndices()) {
				wtr.node("componentType", GL_UNSIGNED_SHORT);
			}
			else {
				wtr.node("componentType", GL_UNSIGNED_INT);
			}
			wtr.node("count", store.GetMeshLength(i));
			wtr.writeArray("min", store.GetMeshOffset(i));
			wtr.writeArray("max", store.GetMeshOffset(i) + store.GetMeshLength(i));
			wtr.node("type", "SCALAR");
			wtr.node("bufferView", 3);
			wtr.node("byteOffset", (int)(store.GetMeshOffset(i) * store.GetIndicesSize()));
			wtr.endObject();
		}
	}
	wtr.endArray();

	wtr.beginObject("asset");
	wtr.node("generator", "glTF plugin for metasequia");
	wtr.node("version", "2.0");
	wtr.endObject();

	/* output buffer/bufferview */
	wtr.beginArray("buffers");
#if MULTI_BUFFER
	{
		std::string  bufname_vc = fname_top + "_vector";

		int buffer_len = store.GetVectorDataSize();
		dbg_pr("buffer_vec: %s(%d)", bufname_vc.c_str(), buffer_len);

		char* buffer = new char[buffer_len];
		int res = store.GetBufferContent(buffer, BUFTYPE_VECTOR);
		dbg_pr("bufres=%d\n", res);
		ExportBuffer(wtr, bufname_vc, buffer, buffer_len, !buftype_bin);
		delete[] buffer;

		std::string bufname_sc = fname_top + "_scalar";
		buffer_len = store.GetScalarDataSize();
		dbg_pr("buffer_sc: %s(%d)", bufname_sc.c_str(), buffer_len);

		buffer = new char[buffer_len];
		res = store.GetBufferContent(buffer, BUFTYPE_SCALAR);
		dbg_pr("bufres=%d\n", res);
		ExportBuffer(wtr, bufname_sc, buffer, buffer_len, !buftype_bin);
		delete[] buffer;
	}
#else
	{
		int  buffer_len = store.GetBufferLength();
		std::vector<char>  buffer = std::vector<char>(buffer_len);
		store.GetBufferContent(buffer);

		ExportBuffer(wtr, fname_top, buffer, buffer_len, !buftype_bin);
	}
#endif
	wtr.endArray();

	//bufferview
	wtr.beginArray("bufferViews");

	wtr.beginObject();
	wtr.node("name", "bufferView_0");
	wtr.node("buffer", 0);
	wtr.node("byteLength", store.GetPositionSize() * 12);
	wtr.node("byteOffset", 0);
	wtr.node("byteStride", 12);
	wtr.node("target", 34962);
	wtr.endObject();

	wtr.beginObject();
	wtr.node("name", "bufferView_1");
	wtr.node("buffer", 0);
	wtr.node("byteLength", store.GetNormalSize() * 12);
	wtr.node("byteOffset", store.GetPositionSize() * 12);
	wtr.node("byteStride", 12);
	wtr.node("target", 34962);
	wtr.endObject();

	wtr.beginObject();
	wtr.node("name", "bufferView_2");
	wtr.node("buffer", 0);
	wtr.node("byteLength", store.GetTexCoordSize() * 8);
	wtr.node("byteOffset", (store.GetPositionSize() + store.GetNormalSize()) * 12);
	wtr.node("byteStride", 8);
	wtr.node("target", 34962);
	wtr.endObject();

	wtr.beginObject();
	wtr.node("name", "bufferView_3");
#if MULTI_BUFFER
	wtr.node("buffer", 1);
	wtr.node("byteLength", store.GetScalarDataSize());
	wtr.node("byteOffset", 0);
#else
	wtr.node("buffer", 0);
	wtr.node("byteLength", store.GetScalarDataSize());
	wtr.node("byteOffset", store.GetVectorDataSize());
#endif
	wtr.node("target", 34963);
	wtr.endObject();

	wtr.endArray();
	//matrials
	wtr.beginArray("materials");

	for (int mat_idx = 0; mat_idx <= numMat; mat_idx++) {
		// Get a material name

		std::string  mat_name_utf8s;
		char mat_name[64];
		MQMaterial mat = doc->GetMaterial(mat_idx);
		if (mat == NULL) continue;

		mat->GetName(mat_name, _countof(mat_name));
		mat_name_utf8s = MQEncoding::AnsiToUtf8(mat_name);

		wtr.beginObject();
		wtr.node("name", mat_name_utf8s.c_str());

		char  shader_name[16];
		mat->GetShaderName(shader_name, 16);
		int numSdr = mat->GetShaderParameterNum();

#if DEBUG
		dbg_pr("shadername: [%s](param:%d)\n", shader_name, numSdr);
		for (int param_idx = 0; param_idx < numSdr; param_idx++) {
			char  param_name[256];
			mat->GetShaderParameterName(param_idx, param_name, 256);
			dbg_pr("\tparam(%d): %s\n", param_idx, param_name);
			dbg_pr("\ttype=%d\n", mat->GetShaderParameterValueType(param_idx));
		}
#endif

		if (!strcmp(shader_name, "glTF")) {
			float  fv;
			int    alpha_mode;
			MQColorRGBA  base_col, emi_col;
			wtr.beginObject("pbrMetallicRoughness");
			
			base_col = mat->GetShaderParameterColorValue("BaseColor", 0);
			wtr.writeArray("baseColorFactor", base_col.r, base_col.g, base_col.b, base_col.a);
			ExportTexture(&wtr, &store, mat_idx, mat, doc);

			fv = mat->GetShaderParameterFloatValue("Metallic", 0);
			wtr.node("metallicFactor", fv);

			fv = mat->GetShaderParameterFloatValue("Roughness", 0);
			wtr.node("roughnessFactor", fv);
			wtr.endObject();
			
			emi_col = mat->GetShaderParameterColorValue("Emissive", 0);
			wtr.writeArray("emissiveFactor", emi_col.r, emi_col.g, emi_col.b);

			const char  alphamode[4][7] = { "OPAQUE", "OPAQUE", "MASK", "BLEND" };
			alpha_mode = mat->GetShaderParameterIntValue("AlphaMode", 0);
			if ((alpha_mode == 0) && (base_col.a < 1.0)) { alpha_mode = 3; }
			wtr.node("alphaMode", alphamode[alpha_mode]);

			if (alpha_mode == 2 /* MASK mode */) {
				fv = mat->GetShaderParameterFloatValue("AlphaCutOff", 0);
				wtr.node("alphaCutoff", fv);
			}
		}
		else {
			dbg_pr("material\n");
			wtr.beginObject("pbrMetallicRoughness");
			dbg_pr("\tcolor\n");
			MQColor  col = mat->GetColor();
			dbg_pr("\talpha\n");
			float alpha = mat->GetAlpha();
			wtr.writeArray("baseColorFactor", col.r, col.g, col.b, alpha);
			ExportTexture(&wtr, &store, mat_idx, mat, doc);
			wtr.node("metallicFactor", 0);
			wtr.node("roughnessFactor", 0.995f);
			wtr.endObject();
			wtr.writeArray("emissiveFactor", 0, 0, 0);
			wtr.node("alphaMode", (alpha==1.0)?"OPAQUE":"BLEND");
		}
		wtr.node("doubleSided", (mat->GetDoubleSided())?true:false);
		wtr.endObject();
	}

	wtr.endArray();

	if (store.GetImageNum() > 0) {
		wtr.beginArray("images");
		for (int img_idx = 0; img_idx < store.GetImageNum(); img_idx++) {
			dbg_pr("\timg_uri=%d\n", img_idx);
			wtr.beginObject();
			wtr.node("uri", store.GetImageUri(img_idx).c_str());
			wtr.endObject();
		}
		wtr.endArray();
	}

	dbg_pr("TexNum=%d\n", store.GetTextureNum());
	if (store.GetTextureNum() > 0) {
		wtr.beginArray("textures");
		for (int idx = 0; idx < store.GetTextureNum(); idx++) {
			Texture  *tex = store.GetTexture(idx);
			wtr.beginObject();
			wtr.node("source", tex->GetImage());
			wtr.node("sampler", tex->GetSampler());
			wtr.endObject();
		}
		wtr.endArray();
	}

	if (store.GetSamplerNum() > 0) {
		wtr.beginArray("samplers");
		for (int idx = 0; idx < store.GetSamplerNum(); idx++) {
			Sampler* smp = store.GetSampler(idx);
			wtr.beginObject();

#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
			const int filter_param[] = { GL_NEAREST, GL_LINEAR };

			wtr.node("magFilter", filter_param[smp->filter]);
			wtr.node("minFilter", filter_param[smp->filter]);

#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_MIRRORED_REPEAT                0x8370
			const int wrap_param[] = { GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE };
			wtr.node("wrapS", wrap_param[smp->wrapS]);
			wtr.node("wrapT", 10497);

			wtr.endObject();
		}
		wtr.endArray();
	}

	//meshes
	wtr.beginArray("meshes");
	wtr.beginObject();
	wtr.node("name", "Node-Mesh");
	wtr.beginArray("primitives");
	{
		int mesh_idx;
		for (mesh_idx = 0; mesh_idx < store.GetMeshNum(); mesh_idx++) {
			wtr.beginObject();
			wtr.beginObject("attributes");
			wtr.node("POSITION", position_idx);
			wtr.node("NORMAL", position_idx + 1);
			wtr.node("TEXCOORD_0", position_idx + 2);
			wtr.endObject();
			wtr.node("indices", mesh_idx + 3);
			wtr.node("material", store.GetMaterialIndex(mesh_idx));
			wtr.node("mode", 4);
			wtr.endObject();
		}
	}
	wtr.endArray();
	wtr.endObject();
	wtr.endArray();

	//nodes
	wtr.beginArray("nodes");
	wtr.beginObject();
	wtr.node("name", "Node");
	wtr.node("mesh", 0);
	wtr.endObject();
	wtr.endArray();

	//scene
	wtr.node("scene", 0);
	wtr.beginArray("scenes");
	wtr.beginObject();
	wtr.writeArray("nodes", 0);
	wtr.endObject();
	wtr.endArray();

	wtr.endObject();

#if DEBUG
	fclose(dbgf);
#endif
	int ret = wtr.close();
	return (ret == 0) ? TRUE : FALSE;
}




MQBasePlugin *GetPluginClass()
{
	static GltfExporter plugin;
	return &plugin;
}

