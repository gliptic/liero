#ifndef UUID_DFBC6E7713BD45652D22AB8D4A57781B
#define UUID_DFBC6E7713BD45652D22AB8D4A57781B

#define FOREACH(t_, i_, c_) for(t_::iterator i_ = (c_).begin(); i_ != (c_).end(); ++i_)
#define CONST_FOREACH(t_, i_, c_) for(t_::const_iterator i_ = (c_).begin(); i_ != (c_).end(); ++i_)
#define REVERSE_FOREACH(t_, i_, c_) for(t_::reverse_iterator i_ = (c_).rbegin(); i_ != (c_).rend(); ++i_)
#define FOREACH_DELETE(t_, v_, c_) for(t_::iterator v_ = (c_).begin(), n_; v_ != (c_).end() && (n_ = v_, ++n_, true); v_ = n_)

// Borrowed from boost
#define GVL_CONCAT(a, b) GVL_CONCAT_I(a, b)
#define GVL_CONCAT_I(a, b) GVL_CONCAT_II(a ## b)
#define GVL_CONCAT_II(res) res

#endif // UUID_DFBC6E7713BD45652D22AB8D4A57781B
