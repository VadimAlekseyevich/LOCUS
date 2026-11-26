#include "app/async_player_summary.h"
#include <iomanip>
#include <locale>
#include <sstream>

namespace locus::app {
bool AsyncPlayerSummary::Update(PlayerProfileView& view) {
    auto& label=view.label;
    bool changed=false;
    if (worker_.valid()) {
        if (worker_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return false;
        Outcome result;
        try { result=worker_.get(); }
        catch (...) { result.error="Profile worker failed."; }
        cancel_.reset();
        if (!dirty_) {
            ++view.revision;
            if (!result.success) { view.state=ProfileLoadState::Failed; label="LOCAL PROFILE UNAVAILABLE"; }
            else {
                view.state=ProfileLoadState::Ready;
                view.snapshot=std::move(result.snapshot);
                view.page=std::min(view.page,view.PageCount()-1);
                const auto& summary=view.snapshot.summary;
                std::ostringstream text;
                text.imbue(std::locale::classic());
                text<<"LOCAL  /  "<<std::fixed<<std::setprecision(1)<<summary.totalPp
                    <<" PP  /  "<<summary.playCount<<" PLAYS";
                if (summary.playCount)
                    text<<"  /  "<<std::setprecision(2)<<summary.weightedAccuracy<<"%";
                label=text.str();
            }
            return true;
        }
    }
    if (dirty_) {
        dirty_=false;
        label="LOADING LOCAL PROFILE";
        view.state=ProfileLoadState::Loading;
        ++view.revision;
        try {
            auto stop=std::make_shared<std::atomic<bool>>(false);
            worker_=std::async(std::launch::async,[store=store_,stop]() {
                Outcome result;
                result.success=store.LoadPlayerProfile(result.snapshot,result.error,stop.get());
                return result;
            });
            cancel_=std::move(stop);
        } catch (...) { view.state=ProfileLoadState::Failed; label="LOCAL PROFILE UNAVAILABLE"; }
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
