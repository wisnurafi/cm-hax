#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Resolved on first use. Both paths land on the current user's desktop, with
// a fallback to the working directory when USERPROFILE isn't available.
static char g_logPath[MAX_PATH] = {};
static char g_outPath[MAX_PATH] = {};
static bool g_pathsResolved     = false;

static void ResolvePaths() {
    if (g_pathsResolved) return;
    g_pathsResolved = true;

    char userProfile[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        snprintf(g_logPath, sizeof(g_logPath), "%s\\Desktop\\dumper_log.txt", userProfile);
        snprintf(g_outPath, sizeof(g_outPath), "%s\\Desktop\\il2cpp_dump.cs", userProfile);
    } else {
        snprintf(g_logPath, sizeof(g_logPath), "dumper_log.txt");
        snprintf(g_outPath, sizeof(g_outPath), "il2cpp_dump.cs");
    }
}

static const char* LogPath() { ResolvePaths(); return g_logPath; }
static const char* OutPath() { ResolvePaths(); return g_outPath; }

static FILE* g_log = NULL;
void Log(const char* fmt, ...) {
    if (!g_log) g_log = fopen(LogPath(), "w");
    if (!g_log) return;
    va_list a; va_start(a, fmt);
    vfprintf(g_log, fmt, a); va_end(a);
    fprintf(g_log, "\n"); fflush(g_log);
}

// ============================================================
// IL2CPP Type Definitions (from UC post - Unity 6000.0.60)
// ============================================================
typedef void(*Il2CppMethodPointer)();
typedef void(*InvokerMethod)(Il2CppMethodPointer, const void*, void*, void**, void*);

struct Il2CppType;
struct Il2CppClass;
struct Il2CppImage;
struct Il2CppAssembly;

struct Il2CppGenericInst { uint32_t type_argc; const Il2CppType** type_argv; };
struct Il2CppGenericContext { const Il2CppGenericInst* class_inst; const Il2CppGenericInst* method_inst; };
struct Il2CppGenericClass { const Il2CppType* type; Il2CppGenericContext context; Il2CppClass* cached_class; };
struct Il2CppArrayType { const Il2CppType* etype; uint8_t rank; uint8_t numsizes; uint8_t numlobounds; int* sizes; int* lobounds; };

struct Il2CppType {
    union { void* dummy; int32_t __klassIndex; uint64_t typeHandle; const Il2CppType* type;
            Il2CppArrayType* array; int32_t __genericParameterIndex; uint64_t genericParameterHandle;
            Il2CppGenericClass* generic_class; } data;
    unsigned int attrs;
    unsigned int type;  // Il2CppTypeEnum
    unsigned int num_mods;
    unsigned int byref;
    unsigned int pinned;
    unsigned int valuetype;
};

struct MethodInfo {
    Il2CppMethodPointer methodPointer;
    Il2CppMethodPointer virtualMethodPointer;
    InvokerMethod invoker_method;
    const char* name;
    Il2CppClass* klass;
    Il2CppType* return_type;
    Il2CppType** parameters;
    union { const void* rgctx_data; uint64_t methodMetadataHandle; };
    union { const void* genericMethod; uint64_t genericContainerHandle; };
    uint32_t token;
    uint16_t flags;
    uint16_t iflags;
    uint16_t slot;
    uint8_t parameters_count;
    uint8_t is_generic; uint8_t is_inflated; uint8_t wrapper_type;
    uint8_t has_full_generic_sharing_signature; uint8_t is_unmanaged_callers_only;
};

struct FieldInfo {
    const char* name;
    Il2CppType* type;
    Il2CppClass* parent;
    int32_t offset;
    uint32_t token;
};

struct PropertyInfo {
    Il2CppClass* parent;
    const char* name;
    const MethodInfo* get;
    const MethodInfo* set;
    uint32_t attrs;
    uint32_t token;
};

struct Il2CppAssemblyName {
    const char* name; const char* culture; const uint8_t* public_key;
    uint32_t hash_alg; int32_t hash_len; uint32_t flags;
    int32_t major; int32_t minor; int32_t build; int32_t revision;
};

struct Il2CppImage {
    const char* name;
    const char* nameNoExt;
    Il2CppAssembly* assembly;
    uint32_t typeCount;
    uint32_t exportedTypeCount;
    uint32_t customAttributeCount;
};

struct Il2CppAssembly {
    Il2CppImage* image;
    uint32_t token;
    int32_t referencedAssemblyStart;
    int32_t referencedAssemblyCount;
    Il2CppAssemblyName aname;
};

struct VirtualInvokeData { Il2CppMethodPointer methodPtr; const MethodInfo* method; };

struct Il2CppClass {
    const Il2CppImage* image;
    void* gc_desc;
    const char* name;
    const char* namespaze;
    Il2CppType byval_arg;
    Il2CppType this_arg;
    Il2CppClass* element_class;
    Il2CppClass* castClass;
    Il2CppClass* declaringType;
    Il2CppClass* parent;
    Il2CppGenericClass* generic_class;
    uint64_t typeMetadataHandle;
    const void* interopData;
    Il2CppClass* klass;
    FieldInfo* fields;
    const void* events;
    const PropertyInfo* properties;
    const MethodInfo** methods;
    Il2CppClass** nestedTypes;
    Il2CppClass** implementedInterfaces;
    void* interfaceOffsets;
    void* static_fields;
    const void* rgctx_data;
    Il2CppClass** typeHierarchy;
    void* unity_user_data;
    void* initializationExceptionGCHandle;
    uint32_t cctor_started;
    uint32_t cctor_finished_or_no_cctor;
    alignas(8) size_t cctor_thread;
    uint64_t genericContainerHandle;
    uint32_t instance_size;
    uint32_t stack_slot_size;
    uint32_t actualSize;
    uint32_t element_size;
    int32_t native_size;
    uint32_t static_fields_size;
    uint32_t thread_static_fields_size;
    int32_t thread_static_fields_offset;
    uint32_t flags;
    uint32_t token;
    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;
    uint8_t typeHierarchyDepth;
    uint8_t genericRecursionDepth;
    uint8_t rank;
    uint8_t minimumAlignment;
    uint8_t packingSize;
    uint8_t initialized_and_no_error;
    uint8_t initialized;
    uint8_t enumtype;
    uint8_t nullabletype;
    uint8_t is_generic;
    uint8_t has_references;
    uint8_t init_pending;
    uint8_t size_init_pending;
    uint8_t size_inited;
    uint8_t has_finalize;
    uint8_t has_cctor;
    uint8_t is_blittable;
    uint8_t is_import_or_windows_runtime;
    uint8_t is_vtable_initialized;
    uint8_t is_byref_like;
    VirtualInvokeData vtable[0];
};

// Field attribute defines
#define FIELD_ATTRIBUTE_STATIC      0x0010
#define FIELD_ATTRIBUTE_PUBLIC      0x0006
#define FIELD_ATTRIBUTE_PRIVATE     0x0001
#define FIELD_ATTRIBUTE_FIELD_ACCESS_MASK 0x0007

// Method attributes
#define METHOD_ATTRIBUTE_STATIC    0x0010
#define METHOD_ATTRIBUTE_PUBLIC    0x0006
#define METHOD_ATTRIBUTE_PRIVATE   0x0001
#define METHOD_ATTRIBUTE_VIRTUAL   0x0040
#define METHOD_ATTRIBUTE_ABSTRACT  0x0400
#define METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK 0x0007

// ============================================================
// IL2CPP API Function Pointers (from UC export mapping)
// ============================================================
typedef void*       (*t_domain_get)();
typedef void*       (*t_thread_attach)(void* domain);
typedef void        (*t_thread_detach)(void* thread);
typedef void**      (*t_domain_get_assemblies)(void* domain, size_t* count);
typedef Il2CppClass* (*t_class_from_name)(Il2CppImage* image, const char* ns, const char* name);
typedef void*       (*t_class_get_methods)(Il2CppClass* klass, void** iter);
typedef void*       (*t_class_get_fields)(Il2CppClass* klass, void** iter);
typedef Il2CppClass* (*t_image_get_class)(Il2CppImage* image, size_t index);
typedef size_t      (*t_image_get_class_count)(Il2CppImage* image);
typedef const char* (*t_type_get_name)(Il2CppType* type);

static t_domain_get             fn_domain_get;
static t_thread_attach          fn_thread_attach;
static t_thread_detach          fn_thread_detach;
static t_domain_get_assemblies  fn_domain_get_assemblies;
static t_class_get_methods      fn_class_get_methods;
static t_class_get_fields       fn_class_get_fields;
static t_image_get_class        fn_image_get_class;
static t_image_get_class_count  fn_image_get_class_count;
static t_type_get_name          fn_type_get_name;

bool ResolveAPIs(HMODULE hProject) {
    #define RESOLVE(var, name) var = (decltype(var))GetProcAddress(hProject, name); \
        Log("  " #var " = %p (%s)", (void*)var, name); \
        if (!var) { Log("  FAILED to resolve " name "!"); return false; }

    RESOLVE(fn_domain_get,            "ApzldXylDWR");
    RESOLVE(fn_thread_attach,         "ZxbKYsbhTwf");
    RESOLVE(fn_thread_detach,         "OoiRpAhKmoS");
    RESOLVE(fn_domain_get_assemblies, "gobBtMyApTj");
    RESOLVE(fn_class_get_methods,     "wwlPDayBOUV");
    RESOLVE(fn_class_get_fields,      "dvnHzpFiKhe");
    RESOLVE(fn_image_get_class,       "sEfnFqjmWJN");
    RESOLVE(fn_image_get_class_count, "eTLADjuMyXv");
    RESOLVE(fn_type_get_name,         "ySfCbKcrsLP");
    #undef RESOLVE
    return true;
}

// ============================================================
// Type name helper
// ============================================================
const char* GetAccessStr(uint16_t flags, bool isField) {
    uint16_t access = flags & (isField ? FIELD_ATTRIBUTE_FIELD_ACCESS_MASK : METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK);
    switch(access) {
        case 0: return "private";
        case 1: return "private";
        case 2: return "protected internal";
        case 3: return "internal";
        case 4: return "protected";
        case 5: return "protected internal";
        case 6: return "public";
        default: return "";
    }
}

const char* SafeTypeName(Il2CppType* type) {
    if (!type) return "void";
    __try {
        const char* n = fn_type_get_name(type);
        if (n && (uintptr_t)n > 0x10000) return n;
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return "???";
}

// ============================================================
// Main dump
// ============================================================
void DoDump() {
    HMODULE hProject = GetModuleHandleA("Project.dll");
    if (!hProject) { Log("No Project.dll!"); return; }
    Log("Project.dll = %p", (void*)hProject);

    if (!ResolveAPIs(hProject)) return;

    // 1. Get domain
    void* domain = fn_domain_get();
    Log("domain = %p", domain);
    if (!domain) return;

    // 2. Attach thread (CRITICAL - prevents GC crash)
    void* thread = fn_thread_attach(domain);
    Log("thread = %p", thread);
    if (!thread) { Log("thread_attach failed!"); return; }

    // 3. Get assemblies
    size_t asmCount = 0;
    void** assemblies = fn_domain_get_assemblies(domain, &asmCount);
    Log("assemblies = %p, count = %llu", assemblies, (unsigned long long)asmCount);
    if (!assemblies || asmCount == 0) { Log("No assemblies!"); return; }

    // 4. Open output
    FILE* out = fopen(OutPath(), "w");
    if (!out) { Log("Can't open output!"); return; }
    fprintf(out, "// IL2CPP Runtime Dump - Combat Master\n");
    fprintf(out, "// Exports: ApzldXylDWR=domain_get, ZxbKYsbhTwf=thread_attach\n");
    fprintf(out, "// gobBtMyApTj=domain_get_assemblies, sEfnFqjmWJN=image_get_class\n");
    fprintf(out, "// eTLADjuMyXv=image_get_class_count, wwlPDayBOUV=class_get_methods\n");
    fprintf(out, "// dvnHzpFiKhe=class_get_fields\n\n");

    int totalClasses = 0, totalMethods = 0, totalFields = 0;

    for (size_t a = 0; a < asmCount; a++) {
        __try {
            Il2CppAssembly* assembly = (Il2CppAssembly*)assemblies[a];
            if (!assembly) continue;
            Il2CppImage* image = assembly->image;
            if (!image) continue;

            const char* imgName = image->name ? image->name : "???";
            size_t classCount = fn_image_get_class_count(image);

            Log("  [%llu] %s: %llu classes", (unsigned long long)a, imgName, (unsigned long long)classCount);
            fprintf(out, "\n// ================================================================\n");
            fprintf(out, "// Image: %s (%llu classes)\n", imgName, (unsigned long long)classCount);
            fprintf(out, "// ================================================================\n\n");

            for (size_t c = 0; c < classCount; c++) {
                __try {
                    Il2CppClass* klass = fn_image_get_class(image, c);
                    if (!klass) continue;

                    const char* className = klass->name ? klass->name : "???";
                    const char* classNs = klass->namespaze ? klass->namespaze : "";

                    // Namespace
                    if (classNs[0])
                        fprintf(out, "namespace %s\n{\n", classNs);

                    // Class header
                    bool isInterface = (klass->flags & 0x20) != 0;
                    bool isAbstract = (klass->flags & 0x80) != 0;
                    bool isSealed = (klass->flags & 0x100) != 0;
                    bool isEnum = klass->enumtype;
                    
                    fprintf(out, "    // TypeDefIndex: %u | Instance Size: 0x%X (%u)\n", 
                            klass->token & 0x00FFFFFF, klass->instance_size, klass->instance_size);
                    
                    if (klass->parent && klass->parent->name)
                        fprintf(out, "    // Parent: %s%s%s\n", 
                            klass->parent->namespaze ? klass->parent->namespaze : "",
                            klass->parent->namespaze && klass->parent->namespaze[0] ? "." : "",
                            klass->parent->name);
                    
                    fprintf(out, "    %s%s%s%s %s",
                        isAbstract && !isInterface ? "abstract " : "",
                        isSealed && !isAbstract ? "sealed " : "",
                        isInterface ? "interface" : (isEnum ? "enum" : "class"),
                        "",
                        className);

                    if (klass->parent && klass->parent->name && 
                        strcmp(klass->parent->name, "Object") != 0 &&
                        strcmp(klass->parent->name, "ValueType") != 0 &&
                        strcmp(klass->parent->name, "Enum") != 0)
                        fprintf(out, " : %s", klass->parent->name);

                    fprintf(out, " // %s%s%s\n    {\n", classNs, classNs[0] ? "." : "", className);

                    // Fields
                    void* fieldIter = NULL;
                    FieldInfo* field;
                    while ((field = (FieldInfo*)fn_class_get_fields(klass, &fieldIter)) != NULL) {
                        __try {
                            if (!field->name) continue;
                            const char* typeName = SafeTypeName(field->type);
                            bool isStatic = field->type && (field->type->attrs & FIELD_ATTRIBUTE_STATIC);
                            const char* access = GetAccessStr(field->type ? field->type->attrs : 0, true);
                            
                            fprintf(out, "        %s%s %s %s; // 0x%X\n",
                                access, isStatic ? " static" : "", typeName, field->name, field->offset);
                            totalFields++;
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }

                    // Methods
                    void* methodIter = NULL;
                    const MethodInfo* method;
                    while ((method = (const MethodInfo*)fn_class_get_methods(klass, &methodIter)) != NULL) {
                        __try {
                            if (!method->name) continue;
                            const char* retType = SafeTypeName(method->return_type);
                            bool isStatic = (method->flags & METHOD_ATTRIBUTE_STATIC) != 0;
                            bool isVirtual = (method->flags & METHOD_ATTRIBUTE_VIRTUAL) != 0;
                            bool isAbstractM = (method->flags & METHOD_ATTRIBUTE_ABSTRACT) != 0;
                            const char* access = GetAccessStr(method->flags, false);
                            
                            fprintf(out, "        %s%s%s%s %s %s(",
                                access,
                                isStatic ? " static" : "",
                                isVirtual ? " virtual" : "",
                                isAbstractM ? " abstract" : "",
                                retType, method->name);

                            // Parameters (just count, no names for speed)
                            for (int p = 0; p < method->parameters_count; p++) {
                                if (p > 0) fprintf(out, ", ");
                                if (method->parameters && method->parameters[p]) {
                                    const char* ptype = SafeTypeName(method->parameters[p]);
                                    fprintf(out, "%s", ptype);
                                } else {
                                    fprintf(out, "???");
                                }
                            }
                            fprintf(out, "); // RVA: 0x%llX Slot: %d\n",
                                (unsigned long long)((uintptr_t)method->methodPointer - (uintptr_t)hProject),
                                method->slot);
                            totalMethods++;
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }

                    fprintf(out, "    }\n");
                    if (classNs[0])
                        fprintf(out, "}\n");
                    fprintf(out, "\n");
                    totalClasses++;
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Log("  Exception in assembly %llu", (unsigned long long)a);
        }
    }

    fprintf(out, "\n// ================================================================\n");
    fprintf(out, "// SUMMARY: %d classes, %d methods, %d fields\n", totalClasses, totalMethods, totalFields);
    fprintf(out, "// ================================================================\n");
    fclose(out);
    Log("*** DUMPED %d classes, %d methods, %d fields ***", totalClasses, totalMethods, totalFields);
    Log("*** Output: %s ***", OutPath());

    // Detach thread
    fn_thread_detach(thread);
}

DWORD WINAPI WorkThread(LPVOID param) {
    Log("=== V15 - Full IL2CPP dump with known export mapping ===");
    Sleep(20000); // Wait for game to fully init
    
    __try {
        DoDump();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log("FATAL EXCEPTION: 0x%08X", GetExceptionCode());
    }

    Log("=== Done ===");
    if (g_log) { fclose(g_log); g_log = NULL; }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, WorkThread, NULL, 0, NULL);
    }
    return TRUE;
}
