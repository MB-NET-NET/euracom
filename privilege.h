#if !defined(__PRIVILEGE_H)
#define __PRIVILEGE_H

extern int privilege_set_alternate_uid(uid_t uid);
extern int privilege_set_alternate_gid(gid_t gid);
extern int privilege_initialize();
extern BOOLEAN privilege_active();
extern int privilege_leave_priv();
extern int privilege_enter_priv();
extern int privilege_drop_priv();

#endif
