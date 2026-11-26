#include "app/async_player_summary.h"
#include <iomanip>
#include <locale>
#include <sstream>

namespace locus::app {
bool AsyncPlayerSummary::Update(std::string& label) {
    bool changed=false;
    if (worker_.valid()) {
        if (worker_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return false;
        Outcome result;
        try { result=worker_.get(); }
        catch (...) { result.error="Profile worker failed."; }
        cancel_.reset();
        if (!dirty_) {
            if (!result.success) label="LOCAL PROFILE UNAVAILABLE";
            else {
                std::ostringstream text;
                text.imbue(std::locale::classic());
                text<<"LOCAL  /  "<<std::fixed<<std::setprecision(1)<<result.summary.totalPp
                    <<" PP  /  "<<result.summary.playCount<<" PLAYS";
                if (result.summary.playCount)
                    text<<"  /  "<<std::setprecision(2)<<result.summary.weightedAccuracy<<"%";
                label=text.str();
            }
            return true;
        }
    }
    if (dirty_) {
        dirty_=false;
        label="LOADING LOCAL PROFILE";
        try {
            auto stop=std::make_shared<std::atomic<bool>>(false);
            worker_=std::async(std::launch::async,[store=store_,stop]() {
                Outcome result;
                result.success=store.LoadPlayerSummary(result.summary,result.error,stop.get());
                return result;
            });
            cancel_=std::move(stop);
        } catch (...) { label="LOCAL PROFILE UNAVAILABLE"; }
        changed=true;
    }
    return changed;
}
void AsyncPlayerSummary::CancelAndJoin() noexcept {
    if (cancel_) cancel_->store(true,std::memory_order_relaxed);
    if (worker_.valid()) { try { (void)worker_.get(); } catch (...) {} }
    cancel_.reset();
}
} // namespace locus::app
